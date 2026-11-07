// SPDX-License-Identifier: MIT
// Jason Page format player (Graftgold games: Fire & Ice, Uridium 2, Virocop, Realms, ...).

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JP_VOICES 4

// The CIA clock: the timebase of JP_TICK_LENGTH.
#define JP_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA rate), in 1 / JP_TIMEBASE_HZ seconds.
#define JP_TICK_LENGTH 14187

#define JP_LOOP_DEPTH 4

typedef struct JpPlayer JpPlayer;

typedef enum JpStatus {
    JpStatus_Ok = 0,
    JpStatus_InvalidData,
    JpStatus_MissingSamples, // the "smp." file next to the song could not be read
    JpStatus_OutOfMemory,
} JpStatus;

typedef struct JpCreateResult {
    JpPlayer* player; // null on failure
    JpStatus status;
} JpCreateResult;

typedef struct JpTickResult {
    bool looped; // every voice reached its loop marker, or the song restarted itself
    bool ended;  // the data sent a voice somewhere the replayer cannot follow; the song stops
} JpTickResult;

// What a voice (0-based) last started: its note (semitones from C-0, -1 if none yet) and the instrument (-1 if none).
typedef struct JpVoiceState {
    int note;
    int instrument;
} JpVoiceState;

// The eagleplayer carries three replayers, one per module format. They share the structure (a track list per voice,
// patterns, an instrument script per note, a Paula register write per tick) but not the module layout or all details.
typedef enum JpFormat {
    JpFormat_None = 0,
    JpFormat_Graftgold, // "format 1": Fire & Ice, Uridium 2, Nipper, Empire Soccer 94; 16-bit offsets in the header
    JpFormat_Virocop,   // "format 2": the same header, a pitch slide with a target, a hold phase in the envelope
    JpFormat_Realms,    // "format 3": absolute pointers, 32-bit tables
} JpFormat;

typedef struct JpMemory {
    uint8_t* data; // size bytes followed by the zero padding the Paula channel needs
    uint32_t size;
} JpMemory;

typedef struct JpLayout {
    uint32_t sample_count;
    uint32_t sample_table;     // the module's table of sample lengths (Graftgold, Virocop) or start offsets (Realms)
    uint32_t* sample_start;    // address of every sample (the sample file is cut by the module's length table)
    uint32_t instrument_table; // per instrument: the offset of its script from script_base (u32 address, Realms)
    uint32_t script_base;      // Graftgold, Virocop
    uint32_t speed_table;      // per subsong: ticks per sequencer step - 1
    uint32_t priority_table;   // per instrument
    uint32_t track_index[JP_VOICES]; // per subsong: the voice's track list (an offset from track_base, or an address)
    uint32_t track_base[JP_VOICES];  // Graftgold, Virocop
    uint32_t pattern_table;          // per pattern: its offset from pattern_base (u32 address, Realms)
    uint32_t pattern_base;           // Graftgold, Virocop
    int subsongs;
} JpLayout;

// An attack or decay phase. The script stores it as one word (rate high byte, ticks low byte); a tick count of 0xFF
// means the phase is over, and a phase counts down through 0 to that.
typedef struct JpEnvelopeRamp {
    uint8_t rate;
    uint8_t ticks;
    bool running;
} JpEnvelopeRamp;

// The hold (ticks) and release (rate per tick) phases: one word each; 0xFFFF in the script means not running.
typedef struct JpEnvelopePhase {
    uint16_t value;
    bool running;
} JpEnvelopePhase;

typedef struct JpVoice {
    NuadePaulaChannel paula;
    uint8_t number;

    // Track list, patterns
    uint16_t track_pos;  // byte offset in the voice's track list
    uint8_t pattern_pos; // byte offset in the pattern
    uint8_t transpose;   // low byte of the track word: the pattern's transposition (the target of a loop word)
    uint16_t last_note;  // note index of the last note
    uint8_t step_count;  // sequencer steps until the next pattern byte
    uint8_t step_reload;
    uint16_t instrument; // pattern instrument number the next notes use
    bool has_pending;    // an instrument is queued to start at the next tick
    uint16_t pending_instrument;
    uint16_t priority; // priority of the sound that plays; a queued instrument needs at least that

    // Instrument script
    uint32_t script;   // address of the next script word
    uint16_t wait;     // ticks until the script runs on
    uint8_t loop_free; // next free slot of the loop stack
    uint8_t loop_top;  // slot of the innermost loop
    uint16_t loop_count[JP_LOOP_DEPTH];
    uint32_t loop_return[JP_LOOP_DEPTH];

    // Pitch
    uint16_t vibrato_depth; // half periods of the vibrato, reloaded
    uint16_t vibrato_count;
    int16_t vibrato_step; // added to the period each tick; negated at every turn
    uint16_t period;
    int16_t slide;             // added to the period each tick
    uint16_t target_period;    // Virocop: the slide stops here
    uint16_t hold_volume;      // Virocop: the volume the hold phase of the envelope lets fall to
    uint16_t hold_volume_next; // Virocop: set by a pattern command, becomes hold_volume on a script command

    // Volume envelope: attack and decay (a rate added to the level for a number of ticks), hold (a tick count) and
    // release (a rate), then the level they act on.
    JpEnvelopeRamp attack;
    JpEnvelopeRamp decay;
    JpEnvelopePhase hold;
    JpEnvelopePhase release;
    uint16_t env_level;
    uint16_t volume; // / 1024 is the Paula volume

    // Sample
    uint32_t sample_base;  // the sample the script selected
    uint32_t sample_start; // base plus what the script added
    uint16_t sample_words;
    uint32_t loop_bytes; // bytes of the sample played from its end; 0 plays nothing

    // Display
    int note;
    int instrument_shown;
} JpVoice;

struct JpPlayer {
    JpFormat format;
    JpLayout layout;
    JpMemory mem;
    uint8_t* pristine;     // the image as loaded (mem.size bytes): a restart goes back to it (scripts rewrite samples)
    uint32_t idle_script;  // address of the script of a voice that has no sound
    uint32_t empty_buffer; // address of the silent word a stopped voice's Paula registers point at

    // The song
    int subsong;
    bool active;           // false once the song hit its end word
    uint16_t speed_reload; // ticks per sequencer step - 1
    int16_t speed_count;
    uint32_t track_list[JP_VOICES];
    JpVoice voices[JP_VOICES];
    bool voice_looped[JP_VOICES]; // the voice passed its loop marker since the last time all of them had
    bool looped;                  // reported by the tick
    bool failed;                  // the data sent a voice out of what the replayer can follow
};

// True when the data passes the eagleplayer's module check (one of the three formats).
bool jp_probe(const uint8_t* data, size_t size);

// `path` locates the song's sample file ("jpn.name" -> "smp.name", "jpnd.name" -> "jpns.name"); a bare file name looks
// in the current directory.
JpCreateResult jp_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void jp_destroy(JpPlayer* player);

int jp_subsong_count(const JpPlayer* player);
// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool jp_start(JpPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
JpTickResult jp_tick(JpPlayer* player, NuadeVoices* voices);

// Ticks one sequencer step spans for a subsong (0-based); 0 for a bad subsong.
uint32_t jp_grid_ticks(const JpPlayer* player, int subsong);
JpVoiceState jp_voice_state(const JpPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
JpPlayer* jp_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
