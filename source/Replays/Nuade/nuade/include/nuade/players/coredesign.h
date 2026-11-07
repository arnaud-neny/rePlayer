// SPDX-License-Identifier: MIT
// Core Design game sound drivers (Ben Daglish, S.PHIPPS), translated from the UADE eagleplayer "CoreDesign"
// (Wanted Team). The module format and the replayer are described in players/coredesign/.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CD_VOICES 4

// The CIA clock: the timebase of CD_TICK_LENGTH.
#define CD_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / CD_TIMEBASE_HZ seconds.
#define CD_TICK_LENGTH 14187

#define CD_TRACKS (CD_VOICES - 1) // the song's tracks play on voices 1 to 3
#define CD_INSTRUMENT_SLOTS 8     // instruments a track can name at a time

typedef struct CdPlayer CdPlayer;

typedef enum CdStatus {
    CdStatus_Ok = 0,
    CdStatus_InvalidData,
    CdStatus_OutOfMemory,
} CdStatus;

typedef struct CdCreateResult {
    CdPlayer* player;
    CdStatus status;
} CdCreateResult;

typedef struct CdTickResult {
    bool looped; // the driver restarted the song, where the eagleplayer calls SongEnd
    bool ended;  // a sound effect: every voice has stopped
} CdTickResult;

// The song's title, author and notice (empty when the module has none).
typedef struct CdInfo {
    char title[64];
    char author[64];
    char notice[256];
} CdInfo;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct CdVoiceState {
    int note;
    int instrument;
} CdVoiceState;

typedef struct CdMemory {
    uint8_t* data;
    uint32_t size;
} CdMemory;

typedef struct CdHeader {
    uint32_t tick;        // the driver's tick routine
    uint32_t audio_irq;   // the audio interrupt handler
    uint32_t start;       // start a subsong
    uint32_t stop;        // silence everything: the first routine of the code the eagleplayer patches
    uint32_t subsongs;    // number of subsongs
    uint32_t instruments; // the instrument records
    uint32_t instruments_end;
    uint32_t title; // strings
    uint32_t author;
    uint32_t notice;
} CdHeader;

typedef struct CdVersion {
    uint8_t note_offset; // added to note + transposition to index the period table
    // Late drivers: flag bit 1 of an instrument gives the first block's length (`loop_offset` words) and selects the
    // loop. Early driver: every instrument that is not one-shot loops, and the first block is the whole sample.
    bool attack_flag;
    // Late drivers: a sound effect plays on voice 0, or voice 1 when its entry's last word is not 0, with timer 0.
    // Early driver: voice 3, and that word is the timer.
    bool effect_voice_by_param;
} CdVersion;

typedef struct CdTables {
    uint32_t subsongs;        // CD_SUBSONG_SIZE bytes each
    uint32_t songs;           // CD_SONG_SIZE bytes each: a signed word offset from here per track
    uint32_t pattern_offsets; // words: signed offset of each pattern from `pattern_base`
    uint32_t pattern_base;
    uint32_t vibratos; // 8 bytes each: the control byte and seven steps
    uint32_t periods;  // words, indexed by a signed note
    uint32_t silence;  // a zero buffer
} CdTables;

typedef struct CdModule {
    CdMemory memory;
    CdHeader header;
    CdVersion version;
    CdTables tables;
} CdModule;

typedef enum CdEnvelope {
    CdEnvelope_Attack,
    CdEnvelope_Decay,
    CdEnvelope_Sustain,
    CdEnvelope_Release,
} CdEnvelope;

typedef struct CdVoice {
    NuadePaulaChannel paula;
    uint32_t instrument; // offset of the instrument record
    uint16_t period;
    uint8_t volume; // 8 bit: Paula plays volume / 2, from 64 on as 64
    uint16_t timer; // note length: the sustain ends when it reaches 0, 0xFFFF counts no more
    uint32_t block; // the sample block the tick hands to Paula: offset and length in words
    uint16_t block_words;
    CdEnvelope envelope;
    bool active;   // the tick writes the registers and runs the envelope
    bool irq_done; // set when a note starts and after the audio interrupt ran, cleared when the DMA starts
    bool one_shot; // from the instrument, when the note started
    bool has_attack;
    int display_note;
    int display_instrument;
} CdVoice;

typedef struct CdTrack {
    uint32_t order_pos;                 // next byte of the track's order list
    uint32_t pattern_start;             // the current pattern
    uint32_t pattern_pos;               // next byte of the pattern
    uint32_t instrument;                // offset of the instrument record notes use
    uint8_t slots[CD_INSTRUMENT_SLOTS]; // instrument offset from the instrument table, by the number a pattern names
    uint8_t repeats;                    // plays of the current pattern that are left, 1 = this one
    uint8_t transpose;
    bool pattern_ended;
    uint8_t note_ticks;     // ticks to the next event
    bool active;            // the order list has not ended
    uint8_t note;           // last note, as the period table index
    uint8_t vibrato_select; // the vibrato that the next note starts
    uint32_t vibrato_pos;   // next byte of the vibrato's sequence
    uint8_t vibrato_delay;
    uint8_t vibrato_steps;
    bool vibrato_running;
    bool vibrato_restart; // the vibrato starts from the first step of its sequence
    uint8_t vibrato_number;
} CdTrack;

struct CdPlayer {
    CdModule module;
    CdVoice voices[CD_VOICES];
    CdTrack tracks[CD_TRACKS];
    int subsong;
    bool song_playing;
    bool music_active; // at least one track's order list is running
    bool looped;
};

// The eagleplayer's check: an AmigaDOS executable with the `S.PHIPPS` header.
bool cd_probe(const uint8_t* data, size_t size);

// Loads the executable (the eagleplayer's InitPlayer) and locates the driver's tables in its code.
CdCreateResult cd_create(const uint8_t* data, size_t size);
void cd_destroy(CdPlayer* player);

int cd_subsong_count(const CdPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool cd_start(CdPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
CdTickResult cd_tick(CdPlayer* player, NuadeVoices* voices);
CdInfo cd_info(const CdPlayer* player);
CdVoiceState cd_voice_state(const CdPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
CdPlayer* cd_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
