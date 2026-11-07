// SPDX-License-Identifier: MIT
// Jesper Olsen player: the module bytes, what the replayers share, and the state of each replayer.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <nuade/players/jesperolsen.h>

// The module bytes: reads past the end return 0 (the replayer would read whatever follows in memory). The allocation
// is a nuade_paula_padded_copy, so a sample that starts inside the module can run past its end with any AUDxLEN.
#define JO_MAX_MODULE_SIZE 0x800000

static inline uint8_t jom_u8(const JoModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static inline uint16_t jom_u16(const JoModule* m, uint32_t offset) {
    if ((uint64_t)offset + 2 <= m->size) {
        return nuade_read_be16(m->data + offset);
    }
    return (uint16_t)((jom_u8(m, offset) << 8) | jom_u8(m, offset + 1));
}

static inline uint32_t jom_u32(const JoModule* m, uint32_t offset) {
    return ((uint32_t)jom_u16(m, offset) << 16) | jom_u16(m, offset + 2);
}

// FNV-1a of the module bytes in [from, to): identifies a replayer that sits in the module
static inline uint32_t jom_hash(const JoModule* m, uint32_t from, uint32_t to) {
    uint32_t hash = 0x811c9dc5u;
    for (uint32_t i = from; i < to; i++) {
        hash = (hash ^ jom_u8(m, i)) * 0x01000193u;
    }
    return hash;
}

// A word the replayer adds to a pointer with adda.w: sign extended
static inline uint32_t jom_offset16(uint16_t value) {
    return (uint32_t)(int32_t)(int16_t)value;
}

// A paula pointer to `offset` into the module. Past the end of the module the replayer's samples read zeros (UADE's
// memory behind the song is clear), as the module's end does with the zero tail of the padded copy behind it.
// AUDxLC ignores bit 0.
static inline const int8_t* jom_sample(const JoModule* m, uint32_t offset) {
    const uint32_t even = offset & ~1u;
    return (const int8_t*)(m->data + (even < m->size ? even : m->size));
}

// A sample start for Paula: AUDxLEN as the replayer writes it (0 plays 65536 words)
static inline void jo_set_sample(const JoModule* m, NuadePaulaChannel* paula, uint32_t offset, uint16_t words) {
    paula->lc = jom_sample(m, offset);
    paula->len_words = words;
}

// A voice record as bytes: replayers B and C keep their state in the module and song commands store into it by
// offset, so each defines its record layout once, as a pair of functions between these bytes and a struct.
static inline void jor_put16(uint8_t* record, uint32_t offset, uint16_t value) {
    record[offset] = (uint8_t)(value >> 8);
    record[offset + 1] = (uint8_t)value;
}

static inline void jor_put32(uint8_t* record, uint32_t offset, uint32_t value) {
    jor_put16(record, offset, (uint16_t)(value >> 16));
    jor_put16(record, offset + 2, (uint16_t)value);
}

// The `size` bytes of the record at `offset` of the module (zeros past its end)
static inline void jom_record(const JoModule* m, uint32_t offset, uint8_t* record, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        record[i] = jom_u8(m, offset + i);
    }
}

// Words and bytes of the song data that every replayer reads

enum {
    JoWord_End = 0x7fff,  // ends a track or a program
    JoWord_Jump = 0x7ffe, // continues a track or a program at the position in the next word
    JoNote_Silent = 0x7f, // a voice with this note plays nothing
};

// The song commands (a byte of 0x80 or more and an argument) in front of a track entry or a row; one limit for all
#define JO_MAX_COMMANDS 0x4000

// A pair of bytes: a program entry, a command and its argument
typedef struct JoPair {
    uint8_t first;
    uint8_t second;
} JoPair;

static inline JoPair jo_read_pair(const JoModule* m, uint32_t at) {
    return (JoPair) { .first = jom_u8(m, at), .second = jom_u8(m, at + 1) };
}

// A counter that runs down a tick at a time: true when it ran out (went below zero)
static inline bool jo_counter_expired(int8_t* counter) {
    *counter = (int8_t)(*counter - 1);
    return *counter < 0;
}

// An accumulator that adds a step every tick: true on a carry
static inline bool jo_accumulate(uint8_t* accumulator, uint8_t step) {
    const unsigned sum = (unsigned)*accumulator + step;
    *accumulator = (uint8_t)sum;
    return sum > 0xff;
}

// What follows a position in a track or a program
typedef enum JoFollow {
    JoFollow_End,    // the end word
    JoFollow_Entry,  // an entry
    JoFollow_Jumped, // an entry behind a jump word (the position moved)
} JoFollow;

// The entry at the word offset `*position`, behind a jump word if there is one (then `*position` is the target)
static inline JoFollow jo_follow(const JoModule* m, uint16_t* position, uint32_t* at) {
    *at = jom_offset16(*position);
    if (jom_u16(m, *at) == JoWord_End) {
        return JoFollow_End;
    }
    if (jom_u16(m, *at) != JoWord_Jump) {
        return JoFollow_Entry;
    }
    *position = jom_u16(m, *at + 2);
    *at = jom_offset16(*position);
    return JoFollow_Jumped;
}

// Stores a song command word (command byte, argument) into a voice
typedef void (*JoCommandStore)(void* voice, uint16_t command);

// Runs the song commands at `at`; `store` gets each command word. Returns the position after them.
static inline uint32_t jo_run_commands(const JoModule* m, uint32_t at, void* voice, JoCommandStore store) {
    for (int n = 0; n < JO_MAX_COMMANDS && jom_u8(m, at) >= 0x80; n++, at += 2) {
        store(voice, jom_u16(m, at));
    }
    return at;
}

// A row of a pattern in replayers B and C: a note and its length, or a hold (0x7e) or rest (0x7f) row with a length
typedef enum JoRowKind {
    JoRowKind_Note,
    JoRowKind_Hold,
    JoRowKind_Rest,
} JoRowKind;

typedef struct JoRow {
    JoRowKind kind;
    uint8_t first;  // the note, or 0x7e / 0x7f
    uint8_t second; // the length
    bool tied;      // a note row whose length has bit 7 set continues the sound without a new start
} JoRow;

static inline JoRow jo_read_row(const JoModule* m, uint32_t at) {
    const uint8_t first = jom_u8(m, at);
    const JoRowKind kind = first < 0x7e ? JoRowKind_Note : (first == 0x7e ? JoRowKind_Hold : JoRowKind_Rest);
    const uint8_t second = jom_u8(m, at + 1);
    return (JoRow) {
        .kind = kind, .first = first, .second = second, .tied = kind == JoRowKind_Note && (second & 0x80) != 0
    };
}

// The pattern ends at a zero word: it restarts, and when its repeat count has run out the track moves on
static inline void jo_pattern_advance(const JoModule* m, uint32_t row_end, uint16_t* pattern_position, int8_t* repeat,
                                      uint16_t* track_position) {
    if (jom_u16(m, row_end) != 0) {
        return;
    }
    *pattern_position = 0;
    if (jo_counter_expired(repeat)) {
        *repeat = 0;
        *track_position += 2;
    }
}

static inline void jo_song_end_start(JoSongEnd* end, const bool has_track[JO_VOICES]) {
    for (int i = 0; i < JO_VOICES; i++) {
        end->initial[i] = has_track[i];
        end->pending[i] = has_track[i];
    }
    end->all_passed = false;
}

static inline void jo_voice_passed(JoSongEnd* end, int voice) {
    end->pending[voice] = false;
    for (int i = 0; i < JO_VOICES; i++) {
        if (end->pending[i]) {
            return;
        }
    }
    for (int i = 0; i < JO_VOICES; i++) {
        end->pending[i] = end->initial[i];
    }
    end->all_passed = true;
}

// A voice reached the end of its track: it goes silent
static inline void jo_voice_stopped(JoSongEnd* end, NuadePaulaChannel* paula, int voice) {
    paula->volume = 0;
    jo_voice_passed(end, voice);
}

// The result of a tick: SongEnd means the song ended when no voice plays on, and looped otherwise
static inline JoTickResult jo_song_end_result(JoSongEnd* end, bool voices_play) {
    const JoTickResult result = { .looped = end->all_passed && voices_play, .ended = end->all_passed && !voices_play };
    end->all_passed = false;
    return result;
}

// The display state of a voice: the note of its period once a note has started
static inline JoVoiceState jo_shown_state(bool playing, uint16_t period, int instrument) {
    if (!playing || period == 0) {
        return (JoVoiceState) { .note = -1, .instrument = -1 };
    }
    return (JoVoiceState) { .note = nuade_period_to_note(period), .instrument = instrument };
}

// JoReplayer C (the Lollypop / Georg Glaxo data modules), jo_c.c

bool jo_c_detect(const JoModule* m);
int jo_c_subsong_count(const JoModule* m);
void jo_c_start(JoC* c, const JoModule* m, int subsong);
JoTickResult jo_c_tick(JoC* c, const JoModule* m, NuadeVoices* voices);
JoVoiceState jo_c_voice_state(const JoC* c, int voice);

// JoReplayer B (the Harald Hardtand data modules), jo_b.c

bool jo_b_detect(const JoModule* m);
void jo_b_start(JoB* b, const JoModule* m, int subsong);
JoTickResult jo_b_tick(JoB* b, const JoModule* m, NuadeVoices* voices);
JoVoiceState jo_b_voice_state(const JoB* b, int voice);

// Replayers A1 (the Guldkornsexpressen title music) and A2 (its game music), jo_a.c. The module starts with the
// replayer's code; its tables are at fixed places of that code, per version.

// The version of the replayer that sits in the module, or null
const JoAVersion* jo_a_detect(const JoModule* m);
int jo_a_subsong_count(const JoAVersion* version, const JoModule* m);
void jo_a_start(JoA* a, const JoAVersion* version, const JoModule* m, int subsong);
JoTickResult jo_a_tick(JoA* a, const JoModule* m, NuadeVoices* voices);
JoVoiceState jo_a_voice_state(const JoA* a, int voice);
