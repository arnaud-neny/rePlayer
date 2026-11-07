// SPDX-License-Identifier: MIT
// Jason C. Brooke player: types shared by the module locator and the two replayer versions.
//
// A module is one game's sound driver: 68k code, tables and song data in one file. The driver keeps its voice
// records and a few globals inside that file; the player keeps them in structs and reads everything else (tables,
// streams, samples) from the module bytes at the offsets the locator found in the code.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/jasonbrooke.h>

#define JB_NOT_FOUND 0xFFFFFFFFu

static inline uint8_t jbm_u8(const JbModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static inline uint16_t jbm_u16(const JbModule* m, uint32_t offset) {
    return (uint16_t)((jbm_u8(m, offset) << 8) | jbm_u8(m, offset + 1));
}

static inline int16_t jbm_s16(const JbModule* m, uint32_t offset) {
    return (int16_t)jbm_u16(m, offset);
}

static inline uint32_t jbm_u32(const JbModule* m, uint32_t offset) {
    return ((uint32_t)jbm_u16(m, offset) << 16) | jbm_u16(m, offset + 2);
}

static inline void jbm_set8(JbModule* m, uint32_t offset, uint8_t value) {
    if (offset < m->size) {
        m->data[offset] = value;
    }
}

static inline void jbm_set16(JbModule* m, uint32_t offset, uint16_t value) {
    jbm_set8(m, offset, (uint8_t)(value >> 8));
    jbm_set8(m, offset + 1, (uint8_t)value);
}

// A paula pointer to `offset` into the module; null past its end. `m` must come from nuade_paula_padded_copy, so that a
// sample that starts inside the module plays zeros past its end.
static inline const int8_t* jbm_sample(const JbModule* m, uint32_t offset) {
    return offset <= m->size ? (const int8_t*)(m->data + offset) : nullptr;
}

// An offset the driver adds to the module address with adda.w: the 16 bit value sign extended
static inline uint32_t jb_offset16(uint16_t value) {
    return (uint32_t)(int32_t)(int16_t)value;
}

// The first match of `pattern` at an even offset in [from, to): words of four hex digits separated by spaces, an "x"
// digit matches any value ("43fa xxxx", "103b 00xx"). JB_NOT_FOUND if there is none.
uint32_t jbm_find(const JbModule* m, uint32_t from, uint32_t to, const char* pattern);

// The voices' song loop detection: the driver's SongEnd patch clears one flag per voice that passed the end of its
// track list and signals the loop when all four are clear. Returns true when `voice` was the last one.
static inline bool jb_voice_wrapped(bool wrapped[JB_VOICES], int voice) {
    wrapped[voice] = true;
    for (int i = 0; i < JB_VOICES; i++) {
        if (!wrapped[i]) {
            return false;
        }
    }
    for (int i = 0; i < JB_VOICES; i++) {
        wrapped[i] = false;
    }
    return true;
}

// A track step reads at most this many bytes before the voice is considered broken (a malformed module would loop
// forever; the driver would too)
#define JB_MAX_STEP_BYTES 0x10000

// JbVersion A: the 1987-90 driver with a PWM waveform, volume envelopes and pitch sequences. The
// code differs between games in table positions, in a handful of instructions and in the command set; the locator
// reads those out of the code.

// Table records of the driver, one authoritative layout each

#define JB_A_SAMPLE_SIZE 10

typedef struct JbASample {
    uint16_t multiplier; // scales the period table value to the sample's pitch
    uint32_t address;    // offset into the module
    uint16_t length;     // bytes
    int16_t loop;        // offset of the loop in bytes; negative = one-shot
} JbASample;

static inline JbASample jb_a_sample(const JbModule* m, const JbATables* t, uint8_t index) {
    const uint32_t record = t->samples + JB_A_SAMPLE_SIZE * (uint32_t)index;
    return (JbASample) { .multiplier = jbm_u16(m, record),
                         .address = jbm_u32(m, record + 2),
                         .length = jbm_u16(m, record + 6),
                         .loop = jbm_s16(m, record + 8) };
}

#define JB_A_SONG_SIZE 0x12

typedef struct JbATrack {
    uint8_t end;   // track list position that ends the list
    uint8_t loop;  // position the list continues at
    uint16_t list; // offset of the track list: one stream offset per position
} JbATrack;

typedef struct JbASong {
    uint8_t tempo;
    JbATrack tracks[JB_VOICES];
} JbASong;

static inline JbASong jb_a_song(const JbModule* m, const JbATables* t, int subsong) {
    const uint32_t record = t->songs + JB_A_SONG_SIZE * (uint32_t)subsong;
    JbASong song = { .tempo = jbm_u8(m, record) };
    for (uint32_t i = 0; i < JB_VOICES; i++) {
        song.tracks[i] = (JbATrack) { .end = jbm_u8(m, record + 1 + 4 * i),
                                      .loop = jbm_u8(m, record + 2 + 4 * i),
                                      .list = jbm_u16(m, record + 3 + 4 * i) };
    }
    return song;
}

// Where the driver keeps its global bytes, from the start of the globals
typedef enum JbAGlobalByte {
    JbAGlobalByte_Master = 1,
    JbAGlobalByte_SlowDown = 3,
    JbAGlobalByte_Tempo = 5,
    JbAGlobalByte_PwmIndex = 7,
    JbAGlobalByte_PwmLow = 8,
    JbAGlobalByte_PwmHigh = 9,
    JbAGlobalByte_PwmPhase = 10,
    JbAGlobalByte_PwmPhase2 = 11,
    JbAGlobalByte_Transpose = 12,
} JbAGlobalByte;

// The global bytes the module file holds (the init routine sets the accumulators, the tempo and the play flag)
static inline JbAGlobals jb_a_globals(const JbModule* m, const JbATables* t) {
    const uint32_t g = t->globals;
    return (JbAGlobals) { .master = jbm_u8(m, g + JbAGlobalByte_Master),
                          .slow_down = jbm_u8(m, g + JbAGlobalByte_SlowDown),
                          .pwm_index = jbm_u8(m, g + JbAGlobalByte_PwmIndex),
                          .pwm_low = jbm_u8(m, g + JbAGlobalByte_PwmLow),
                          .pwm_high = jbm_u8(m, g + JbAGlobalByte_PwmHigh),
                          .pwm_phase = jbm_u8(m, g + JbAGlobalByte_PwmPhase),
                          .pwm_phase2 = jbm_u8(m, g + JbAGlobalByte_PwmPhase2) };
}

// Finds the version A tables in the code at `entry`; false if the code is not a known version.
bool jb_a_locate(const JbModule* m, uint32_t entry, JbAConfig* config);
void jb_a_start(JbA* a, const JbModule* m, int subsong);
JbTickResult jb_a_tick(JbA* a, JbModule* m, NuadeVoices* voices);

// JbVersion B: the early driver (Feud, Pac-Land, World Darts): four voices playing 128-byte blocks of one sample bank.
// Three code variants: Feud (no master scale, byte envelope index, vibrato hold), Pac-Land (master scale, separate
// envelope table and a word index, vibrato hold), World Darts (master scale, byte envelope index, no vibrato hold).

// The command numbers of the handler table (byte 0x80 + n)
typedef enum JbBCommand {
    JbBCommand_KeyOff,
    JbBCommand_ClearFlags,
    JbBCommand_Slide,
    JbBCommand_NoteDown,
    JbBCommand_NoteUp,
    JbBCommand_NextTrack,
    JbBCommand_Vibrato,
    JbBCommand_Stop,
    JbBCommand_Transpose,
    JbBCommand_AdkCon,
    JbBCommand_Count,   // the number of commands
    JbBCommand_Unknown, // a byte 0x8A..0x8F: no handler
} JbBCommand;

#define JB_B_SONG_SIZE 10

typedef struct JbBSong {
    uint8_t tempo;
    uint16_t track_lists[JB_VOICES];
} JbBSong;

static inline JbBSong jb_b_song(const JbModule* m, const JbBTables* t, int subsong) {
    const uint32_t record = t->songs + JB_B_SONG_SIZE * (uint32_t)subsong;
    JbBSong song = { .tempo = jbm_u8(m, record + 1) };
    for (uint32_t i = 0; i < JB_VOICES; i++) {
        song.track_lists[i] = jbm_u16(m, record + 2 + 2 * i);
    }
    return song;
}

bool jb_b_locate(const JbModule* m, JbBConfig* config);
void jb_b_start(JbB* b, const JbModule* m, int subsong);
JbTickResult jb_b_tick(JbB* b, JbModule* m, NuadeVoices* voices);
