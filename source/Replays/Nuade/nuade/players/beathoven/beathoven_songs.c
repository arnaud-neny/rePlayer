// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: the songs of a module. Each module's init routine sets up the four voice records for a
// subsong; this reads what it sets out of its instructions. The routine the eagleplayer runs once after loading is
// here too: it relocates data of the module that the init routines read.
//
// The init routine starts with a dispatch on the subsong number in d0 (tst.w d0, subq.w #1,d0 or cmpi.w #n,d0 and a
// branch each), which leads to the code that sets the four voices up: for every voice it writes the hardware address,
// the track list and the loop entry into the voice record. A module has the addresses in the instructions
// (move.l #track,4(a0)), in a table of eight longwords that the code reads through a2, or in registers loaded from
// two tables. On the way the routine may set words of the module (the tempo, the voices that play). Two modules index
// their tables with the subsong number instead of dispatching.

#include "beathoven_code.h"
#include <stdlib.h>

// The voice set-up writes AUDxLC of channel n (0xDFF0A0 + 16 n) into the voice record, then the address of the track
// list and the loop entry
#define HW_AUDIO_BASE 0xDFF0A0u
#define HW_CHANNEL_STRIDE 0x10u

// A dispatch is a few instructions; the walk stops after this many
#define MAX_DISPATCH_STEPS 64
// A voice's set-up is within this many bytes of the previous one's
#define SETUP_SPAN 0x400u
// A list of records ends at its zero entry; a module without one is broken
#define MAX_LIST_ENTRIES 4096
#define MAX_WRITES 8192

static bool add_write(BthSong* song, uint32_t address, uint16_t value) {
    if (song->write_count == song->write_capacity) {
        const uint32_t capacity = song->write_capacity == 0 ? 8 : song->write_capacity * 2;
        BthWordWrite* grown = capacity > MAX_WRITES ? nullptr : realloc(song->writes, capacity * sizeof(BthWordWrite));
        if (grown == nullptr) {
            return false;
        }
        song->writes = grown;
        song->write_capacity = capacity;
    }
    song->writes[song->write_count++] = (BthWordWrite) { .address = address, .value = value };
    return true;
}

static bool add_write32(BthSong* song, uint32_t address, uint32_t value) {
    return add_write(song, address, (uint16_t)(value >> 16)) && add_write(song, address + 2, (uint16_t)value);
}

void bth_songs_free(BthSong* songs, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        free(songs[i].writes);
    }
}

// A sequential reader of the instruction words of a routine that is matched whole
typedef struct Cursor {
    const BthModule* module;
    uint32_t at;
    bool ok;
} Cursor;

#define REQUIRE(c, words) cursor_require((c), (words), nuade_sizeof_array(words))

// Consumes `count` words, which must be `words`; once a word did not match, nothing is consumed any more
static void cursor_require(Cursor* c, const uint32_t* words, uint32_t count) {
    c->ok = c->ok && bthm_match(c->module, c->at, words, count);
    if (c->ok) {
        c->at += 2 * count;
    }
}

static void cursor_require_word(Cursor* c, uint16_t word) {
    const uint32_t words[] = { word };
    cursor_require(c, words, 1);
}

// Consumes a branch of the given kind with a one byte displacement
static void cursor_require_short_branch(Cursor* c, uint16_t kind) {
    c->ok = c->ok && bthm_is_branch(c->module, c->at, kind) && (bthm_u16(c->module, c->at) & 0xFF) != 0;
    if (c->ok) {
        c->at += 2;
    }
}

static uint32_t cursor_take32(Cursor* c) {
    const uint32_t value = bthm_u32(c->module, c->at);
    c->at += 4;
    return value;
}

static uint32_t cursor_take_pc16(Cursor* c) {
    const uint32_t value = bthm_pc16(c->module, c->at);
    c->at += 2;
    return value;
}

// The routine the eagleplayer calls once when it has loaded the module (the header's `once`). Two forms exist.

// An entry of form A's list: an instrument record and the offset of its sample. The list ends at a record of 0.
#define SAMPLE_LIST_ENTRY_BYTES 8

typedef struct SampleListEntry {
    uint32_t record;
    uint32_t offset;
} SampleListEntry;

static SampleListEntry read_sample_list_entry(const BthModule* m, uint32_t list, uint32_t index) {
    const uint32_t entry = list + SAMPLE_LIST_ENTRY_BYTES * index;
    return (SampleListEntry) { .record = bthm_u32(m, entry), .offset = bthm_u32(m, entry + 4) };
}

// Form A: copy a longword into a variable, then for every (record, offset) pair of a list set the record's sample
// address to the offset plus a base:
//   lea var(pc),a0; move.l src(pc),(a0); lea list(pc),a0; move.l base(pc),d0
//   loop: move.l (a0)+,d1; beq.w end; movea.l d1,a1; move.l (a0)+,d2; add.l d0,d2; move.l d2,sample(a1); bra.b loop
static bool apply_sample_list(BthModule* m, uint32_t at) {
    static const uint32_t loop[] = { OP_MOVE_L_INC_A0_D1,
                                     OP_BEQ,
                                     BTH_ANY,
                                     OP_MOVEA_D1_A1,
                                     OP_MOVE_L_INC_A0_D2,
                                     OP_ADD_L_D0_D2,
                                     OP_MOVE_L_D2_REC_A1,
                                     BTH_INSTRUMENT_SAMPLE_FIELD,
                                     BTH_ANY };
    Cursor c = { .module = m, .at = at, .ok = true };
    cursor_require_word(&c, OP_LEA_PC_A0);
    const uint32_t variable = cursor_take_pc16(&c);
    cursor_require_word(&c, OP_MOVE_L_PC_IND_A0);
    const uint32_t source = cursor_take_pc16(&c);
    cursor_require_word(&c, OP_LEA_PC_A0);
    uint32_t list = cursor_take_pc16(&c);
    cursor_require_word(&c, OP_MOVE_L_PC_D0);
    const uint32_t base_address = cursor_take_pc16(&c);
    REQUIRE(&c, loop);
    if (!c.ok) {
        return false;
    }
    bthm_set32(m, variable, bthm_u32(m, source));
    const uint32_t base = bthm_u32(m, base_address);
    for (uint32_t i = 0; i < MAX_LIST_ENTRIES; i++) {
        const SampleListEntry entry = read_sample_list_entry(m, list, i);
        if (entry.record == 0) {
            break;
        }
        bthm_set32(m, bthm_instrument_sample_address(entry.record), entry.offset + base);
    }
    return true;
}

// Form B's table: the start address, then entries of (gap, address) that follow it
#define ADDRESS_TABLE_LONGWORD_BYTES 4
#define ADDRESS_TABLE_ENTRY_BYTES 8

// Form B: a table of addresses built by adding the gaps stored in every other slot to a start address, and a block
// of words copied:
//   movem.l d0-d7/a0-a6,-(a7); lea start(pc),a4; lea table.l,a3; move.l a4,(a3)+
//   loop: move.l (a3)+,d0; beq.b copy; adda.l d0,a4; move.l a4,(a3)+; bra.b loop
//   copy: lea src(pc),a0; lea dst(pc),a1; move.w #n,d0; copy_loop: move.w (a0)+,(a1)+; dbra d0,copy_loop; ...; rts
static bool apply_address_table(BthModule* m, uint32_t at) {
    static const uint32_t save_all[] = { OP_MOVEM_PUSH, MOVEM_PUSH_D0_A6 };
    static const uint32_t table_head[] = { OP_MOVE_L_A4_INC_A3 };
    static const uint32_t table_loop[]
        = { OP_MOVE_L_INC_A3_D0, BTH_ANY, OP_ADDA_L_D0_A4, OP_MOVE_L_A4_INC_A3, BTH_ANY };
    static const uint32_t copy_loop[]
        = { OP_MOVE_W_INC_A0_A1, OP_DBRA_D0, 0xFFFC, OP_MOVEM_POP, MOVEM_POP_D0_A6, OP_RTS };
    Cursor c = { .module = m, .at = at, .ok = true };
    REQUIRE(&c, save_all);
    cursor_require_word(&c, OP_LEA_PC_A4);
    uint32_t address = cursor_take_pc16(&c);
    cursor_require_word(&c, OP_LEA_ABS_A3);
    uint32_t table = cursor_take32(&c);
    REQUIRE(&c, table_head);
    REQUIRE(&c, table_loop);
    cursor_require_word(&c, OP_LEA_PC_A0);
    const uint32_t source = cursor_take_pc16(&c);
    cursor_require_word(&c, OP_LEA_PC_A1);
    const uint32_t destination = cursor_take_pc16(&c);
    cursor_require_word(&c, OP_MOVE_W_IMM_D0);
    const uint32_t words = (uint32_t)bthm_u16(m, c.at) + 1;
    c.at += 2;
    REQUIRE(&c, copy_loop);
    if (!c.ok) {
        return false;
    }
    bthm_set32(m, table, address);
    for (uint32_t i = 0; i < MAX_LIST_ENTRIES; i++) {
        const uint32_t entry = table + ADDRESS_TABLE_LONGWORD_BYTES + ADDRESS_TABLE_ENTRY_BYTES * i;
        const uint32_t gap = bthm_u32(m, entry);
        if (gap == 0) {
            break;
        }
        address += gap;
        bthm_set32(m, entry + ADDRESS_TABLE_LONGWORD_BYTES, address);
    }
    for (uint32_t i = 0; i < words; i++) {
        bthm_set16(m, destination + 2 * i, bthm_u16(m, source + 2 * i));
    }
    return true;
}

bool bth_apply_once(BthModule* module, const BthHeader* header) {
    if (header->once == 0) {
        return true;
    }
    return apply_sample_list(module, header->once) || apply_address_table(module, header->once);
}

// Where the init routine's dispatch leads for a subsong
typedef struct InitPath {
    uint32_t stop;         // the first instruction that is not part of the dispatch
    uint32_t table;        // the table of track lists the voice set-up reads through a2, 0 for none
    uint32_t tracks_table; // the tables the voice set-up loads registers from, 0 for none
    uint32_t loops_table;
    bool failed; // a word write did not fit in the song's list
} InitPath;

// The state the dispatch tests: the subsong number counted down by subq, the zero flag the last test set, and the
// address the last lea loaded into a0
typedef struct Dispatch {
    uint32_t remaining;
    bool equal;
    uint32_t last_lea;
} Dispatch;

static void write_word(InitPath* path, BthSong* song, uint32_t address, uint16_t value) {
    path->failed = path->failed || !add_write(song, address, value);
}

// An instruction that sets module words or loads a table address: applies it. Returns the address after it, 0 if the
// word at `at` is another instruction.
static uint32_t apply_setup_instruction(const BthModule* m, uint32_t at, Dispatch* d, InitPath* path, BthSong* song) {
    const uint16_t op = bthm_u16(m, at);
    switch (op) {
        case OP_LEA_ABS_A0:
            d->last_lea = bthm_u32(m, at + 2);
            return at + 6;
        case OP_LEA_PC_A0:
            d->last_lea = bthm_pc16(m, at + 2);
            return at + 4;
        case OP_LEA_ABS_A2:
            path->table = bthm_u32(m, at + 2);
            return at + 6;
        case OP_LEA_PC_A2:
            path->table = bthm_pc16(m, at + 2);
            return at + 4;
        case OP_MOVEA_A0_A2:
            path->table = d->last_lea;
            return at + 2;
        case OP_CLR_W_ABS:
            write_word(path, song, bthm_u32(m, at + 2), 0);
            return at + 6;
        case OP_MOVE_W_IMM_ABS:
            write_word(path, song, bthm_u32(m, at + 4), bthm_u16(m, at + 2));
            return at + 8;
        case OP_MOVE_W_D0_ABS:
            write_word(path, song, bthm_u32(m, at + 2), (uint16_t)d->remaining);
            return at + 6;
        case OP_MOVE_W_IMM_A0:
            write_word(path, song, d->last_lea, bthm_u16(m, at + 2));
            return at + 4;
        case OP_MOVE_W_IMM_A0_REC:
            write_word(path, song, d->last_lea + (uint32_t)(int32_t)bthm_s16(m, at + 4), bthm_u16(m, at + 2));
            return at + 6;
        case OP_MOVE_L_IMM_D0:
            d->remaining = bthm_u32(m, at + 2);
            return at + 6;
        case OP_MOVEM_ABS:
            if (bthm_u16(m, at + 2) == MOVEM_A3_A6) {
                path->tracks_table = bthm_u32(m, at + 4);
            } else if (bthm_u16(m, at + 2) == MOVEM_D3_D6) {
                path->loops_table = bthm_u32(m, at + 4);
            } else {
                return 0;
            }
            return at + 8;
        default:
            return 0;
    }
}

// Walks the init routine's dispatch for `subsong`. It follows the branches the subsong number selects (a call is
// followed like a jump) and applies the instructions that set module words or load table addresses; it ends at the
// first instruction that is not part of the dispatch.
static bool follow_dispatch(const BthModule* m, uint32_t start, uint32_t subsong, InitPath* path, BthSong* song) {
    Dispatch d = { .remaining = subsong };
    uint32_t at = start;
    for (int step = 0; step < MAX_DISPATCH_STEPS; step++) {
        const uint16_t op = bthm_u16(m, at);
        uint32_t next = 0;
        if (bthm_is_branch(m, at, OP_BRA) || bthm_is_branch(m, at, OP_BSR)) {
            at = bthm_branch_target(m, at, &next);
        } else if (bthm_is_branch(m, at, OP_BNE)) {
            const uint32_t target = bthm_branch_target(m, at, &next);
            at = d.equal ? next : target;
        } else if (bthm_is_branch(m, at, OP_BEQ)) {
            const uint32_t target = bthm_branch_target(m, at, &next);
            at = d.equal ? target : next;
        } else if (op == OP_TST_W_D0) {
            d.equal = d.remaining == 0;
            at += 2;
        } else if (op == OP_SUBQ_1_D0) {
            d.remaining--;
            d.equal = d.remaining == 0;
            at += 2;
        } else if (op == OP_CMPI_W_D0) {
            d.equal = d.remaining == bthm_u16(m, at + 2);
            at += 4;
        } else if (op == OP_TST_W_A0) {
            d.equal = bthm_u16(m, d.last_lea) == 0;
            at += 2;
        } else {
            next = apply_setup_instruction(m, at, &d, path, song);
            if (next == 0) {
                path->stop = at;
                return !path->failed;
            }
            at = next;
        }
    }
    return false;
}

// The track list and loop entry the voice set-up at `at` stores in the voice record, if it is of a known form: the
// address is an immediate or the contents of a longword of the module
static uint32_t read_field_store(const BthModule* m, uint32_t at, uint16_t field, uint32_t* value) {
    switch (bthm_u16(m, at)) {
        case OP_MOVE_L_IMM_REC:
            if (bthm_u16(m, at + 6) != field) {
                return 0;
            }
            *value = bthm_u32(m, at + 2);
            return at + 8;
        case OP_MOVE_L_PC_REC:
            if (bthm_u16(m, at + 4) != field) {
                return 0;
            }
            *value = bthm_u32(m, bthm_pc16(m, at + 2));
            return at + 6;
        default:
            return 0;
    }
}

// The voice set-up at `at`, the instruction that writes channel `channel`'s hardware address into the record: the
// track list and the loop entry that follow. Returns false if the instructions are not of a known form.
static bool read_voice_setup(const BthModule* m, uint32_t at, uint32_t channel, BthTrackStart* out) {
    const uint32_t hardware = HW_AUDIO_BASE + channel * HW_CHANNEL_STRIDE;
    uint32_t next = 0;
    if (bthm_u16(m, at) == OP_MOVE_L_IMM_REC && bthm_u32(m, at + 2) == hardware && bthm_u16(m, at + 6) == 0) {
        next = at + 8;
    } else if (bthm_u16(m, at) == OP_MOVE_L_IMM_A0 && bthm_u32(m, at + 2) == hardware) {
        next = at + 6;
    } else {
        return false;
    }
    next = read_field_store(m, next, RECORD_TRACK, &out->track);
    return next != 0 && read_field_store(m, next, RECORD_LOOP, &out->loop) != 0;
}

// The first voice set-up of a channel at or after `from`
static bool find_voice_setup(const BthModule* m, uint32_t from, uint32_t channel, BthTrackStart* out) {
    for (uint32_t at = from; at + 8 <= m->size && at < from + SETUP_SPAN; at += 2) {
        if (read_voice_setup(m, at, channel, out)) {
            return true;
        }
    }
    return false;
}

// A table of (track list, loop entry) pairs, one for each voice
#define TABLE_PAIR_BYTES 8

static BthTrackStart read_track_start(const BthModule* m, uint32_t table, uint32_t voice) {
    return (BthTrackStart) { .track = bthm_u32(m, table + TABLE_PAIR_BYTES * voice),
                             .loop = bthm_u32(m, table + TABLE_PAIR_BYTES * voice + 4) };
}

// A subsong's entry in the indexed init's song table: the track list and loop entry of every voice, and two words for
// the module's variables
#define INDEXED_SONG_BYTES 0x24

typedef struct IndexedSong {
    BthTrackStart voices[BTH_VOICES];
    uint16_t tempo;
    uint16_t note_offset;
} IndexedSong;

static IndexedSong read_indexed_song(const BthModule* m, uint32_t entry) {
    IndexedSong song = { 0 };
    for (uint32_t voice = 0; voice < BTH_VOICES; voice++) {
        song.voices[voice] = read_track_start(m, entry, voice);
    }
    song.tempo = bthm_u16(m, entry + TABLE_PAIR_BYTES * BTH_VOICES);
    song.note_offset = bthm_u16(m, entry + TABLE_PAIR_BYTES * BTH_VOICES + 2);
    return song;
}

// A subsong's entry in the indexed init's sample table: a list of sample records (zero-terminated) and the instrument
// records to give them
#define INDEXED_SAMPLES_BYTES 8

typedef struct IndexedSamples {
    uint32_t list;
    uint32_t records;
} IndexedSamples;

static IndexedSamples read_indexed_samples(const BthModule* m, uint32_t entry) {
    return (IndexedSamples) { .list = bthm_u32(m, entry), .records = bthm_u32(m, entry + 4) };
}

// A sample record: the address of the sample, and a length word at +6
#define SAMPLE_LENGTH_FIELD 6

typedef struct SampleInfo {
    uint32_t sample;
    uint16_t length;
} SampleInfo;

static SampleInfo read_sample_info(const BthModule* m, uint32_t record) {
    return (SampleInfo) { .sample = bthm_u32(m, record), .length = bthm_u16(m, record + SAMPLE_LENGTH_FIELD) };
}

// The indexed init of two modules (Cosmic Bouncer, Rock Challenge) indexes its tables with the subsong number:
//   andi.l #$ffff,d0; move.l d0,d1; lsl.l #3,d1; lea samples.l,a0; movea.l 4(a0,d1.l),a1; movea.l (a0,d1.l),a0
//   loop: move.l (a0)+,d2; tst.l d2; beq.b end; movea.l d2,a2; move.l (a2),sample(a1); [move.w 6(a2),length(a1);]
//         adda.w #14,a1; bra.b loop
//   lea songs.l,a0; mulu.w #$24,d0; adda.l d0,a0; lea voices.l,a1; moveq #7,d0; copy the 8 longwords;
//   move.w (a0)+,tempo.l; move.w (a0)+,note_offset.l; ...
static bool decode_indexed(const BthModule* m, uint32_t at, uint32_t subsong, BthSong* song) {
    // the extension words of the two indexed addressing modes: (a0,d1.l) with a displacement of 4 and of 0
    static const uint32_t head[] = { OP_ANDI_L_D0, 0x0000, 0xFFFF, OP_MOVE_L_D0_D1, OP_LSL_L_3_D1 };
    static const uint32_t fetch[]
        = { OP_MOVEA_L_4_A0_D1_A1, 0x1804, OP_MOVEA_L_A0_D1_A0, 0x1800, OP_MOVE_L_INC_A0_D2, OP_TST_L_D2 };
    static const uint32_t sample[] = { OP_MOVEA_D2_A2, OP_MOVE_L_A2_IND_REC_A1, BTH_INSTRUMENT_SAMPLE_FIELD };
    static const uint32_t length[]
        = { OP_MOVE_W_REC_A2_REC_A1, SAMPLE_LENGTH_FIELD, BTH_INSTRUMENT_ONE_SHOT_LENGTH_FIELD };
    static const uint32_t next_record[] = { OP_ADDA_W_IMM_A1, BTH_INSTRUMENT_BYTES };
    static const uint32_t songs_index[] = { OP_MULU_IMM_D0, INDEXED_SONG_BYTES, OP_ADDA_L_D0_A0, OP_LEA_ABS_A1 };
    static const uint32_t copy[] = { OP_MOVEQ_7_D0, OP_MOVE_L_INC_A0_A1, OP_DBRA_D0, 0xFFFC, OP_MOVE_W_INC_A0_ABS };
    static const uint32_t note_offset[] = { OP_MOVE_W_INC_A0_ABS };
    Cursor c = { .module = m, .at = at, .ok = true };
    REQUIRE(&c, head);
    cursor_require_word(&c, OP_LEA_ABS_A0);
    const uint32_t samples = cursor_take32(&c);
    REQUIRE(&c, fetch);
    cursor_require_short_branch(&c, OP_BEQ);
    REQUIRE(&c, sample);
    const bool with_length = c.ok && BTH_MATCH(m, c.at, length);
    if (with_length) {
        c.at += 2 * nuade_sizeof_array(length);
    }
    REQUIRE(&c, next_record);
    cursor_require_short_branch(&c, OP_BRA);
    cursor_require_word(&c, OP_LEA_ABS_A0);
    const uint32_t songs = cursor_take32(&c);
    REQUIRE(&c, songs_index);
    c.at += 4; // the destination of the copy
    REQUIRE(&c, copy);
    const uint32_t tempo = cursor_take32(&c);
    REQUIRE(&c, note_offset);
    const uint32_t note = cursor_take32(&c);
    if (!c.ok) {
        return false;
    }
    const IndexedSamples entry = read_indexed_samples(m, samples + INDEXED_SAMPLES_BYTES * subsong);
    for (uint32_t i = 0; i < MAX_LIST_ENTRIES && bthm_u32(m, entry.list + 4 * i) != 0; i++) {
        const SampleInfo info = read_sample_info(m, bthm_u32(m, entry.list + 4 * i));
        const uint32_t record = entry.records + BTH_INSTRUMENT_BYTES * i;
        if (!add_write32(song, bthm_instrument_sample_address(record), info.sample)
            || (with_length && !add_write(song, bthm_instrument_one_shot_length_address(record), info.length))) {
            return false;
        }
    }
    const IndexedSong indexed = read_indexed_song(m, songs + INDEXED_SONG_BYTES * subsong);
    for (uint32_t voice = 0; voice < BTH_VOICES; voice++) {
        song->voices[voice] = indexed.voices[voice];
    }
    return add_write(song, tempo, indexed.tempo) && add_write(song, note, indexed.note_offset);
}

// One subsong: the track lists of the four voices
static bool decode_song(const BthModule* m, uint32_t init, uint32_t subsong, BthSong* song) {
    InitPath path = { 0 };
    if (!follow_dispatch(m, init, subsong, &path, song)) {
        return false;
    }
    if (bthm_u16(m, path.stop) == OP_ANDI_L_D0) {
        return decode_indexed(m, path.stop, subsong, song);
    }
    if (path.tracks_table != 0 && path.loops_table != 0) {
        // the voices load their track lists and loop entries into registers from two tables
        for (uint32_t voice = 0; voice < BTH_VOICES; voice++) {
            song->voices[voice] = (BthTrackStart) { .track = bthm_u32(m, path.tracks_table + 4 * voice),
                                                    .loop = bthm_u32(m, path.loops_table + 4 * voice) };
        }
        return true;
    }
    bool in_code = true;
    for (uint32_t channel = 0; channel < BTH_VOICES; channel++) {
        in_code = in_code && find_voice_setup(m, path.stop, channel, &song->voices[channel]);
    }
    if (!in_code) {
        // the voices read their track lists from a table of (track list, loop entry) pairs
        if (path.table == 0) {
            return false;
        }
        for (uint32_t voice = 0; voice < BTH_VOICES; voice++) {
            song->voices[voice] = read_track_start(m, path.table, voice);
        }
    }
    return true;
}

// BEATHOVEN100 sets a voice up with move.l #track,$a(a0); move.l #loop,$e(a0) and has one song. Returns the track
// lists of the voices in the order of the code.
static bool decode_legacy(const BthModule* m, uint32_t voice_count, BthSong* song) {
    // the immediates are at the byte offsets 2 and 10 of the pattern
    static const uint32_t setup[] = { OP_MOVE_L_IMM_REC, BTH_ANY, BTH_ANY, RECORD_LEGACY_TRACK,
                                      OP_MOVE_L_IMM_REC, BTH_ANY, BTH_ANY, RECORD_LEGACY_LOOP };
    const uint32_t track_operand = 2;
    const uint32_t loop_operand = 10;
    uint32_t voice = 0;
    for (uint32_t at = 0; at + 2 * nuade_sizeof_array(setup) <= m->size && voice < voice_count; at += 2) {
        if (BTH_MATCH(m, at, setup)) {
            song->voices[voice++]
                = (BthTrackStart) { .track = bthm_u32(m, at + track_operand), .loop = bthm_u32(m, at + loop_operand) };
        }
    }
    return voice == voice_count;
}

uint32_t bth_decode_songs(const BthImage* image, const BthHeader* header, const BthReplayer* replayer, BthSong* songs,
                          uint32_t capacity) {
    if (replayer->engine == BthEngine_Legacy) {
        return capacity == 1 && header->subsong_count == 1
                       && decode_legacy(&image->module, replayer->voice_count, songs)
                   ? 1
                   : 0;
    }
    if (capacity != header->subsong_count) {
        return 0;
    }
    for (uint32_t song = 0; song < capacity; song++) {
        if (!decode_song(&image->module, header->init, song, &songs[song])) {
            bth_songs_free(songs, capacity);
            return 0;
        }
    }
    return capacity;
}
