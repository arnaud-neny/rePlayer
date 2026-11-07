// SPDX-License-Identifier: MIT
// Jeroen Tel player (the music driver of Jeroen Tel's games, 1989-91), translated from the UADE eagleplayer "Jeroen
// Tel". The replayer is in the eagleplayer; a module is the game's driver image (68k code and the song data in one
// file) that the eagleplayer scans for its tables (periods, patterns, instruments, arpeggio lists, songs). The player
// does the same and runs the eagleplayer's replayer on those tables.

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

#define JT_VOICES 4

// The CIA clock: the timebase of JT_TICK_LENGTH.
#define JT_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / JT_TIMEBASE_HZ seconds.
#define JT_TICK_LENGTH 14187

typedef struct JtPlayer JtPlayer;

typedef struct JtTickResult {
    bool looped; // a track list reached its end mark (the eagleplayer's SongEnd call): every voice starts over
    bool ended;  // a track list reached the stop mark: the replayer plays nothing more
} JtTickResult;

typedef struct JtVoiceState {
    int note;       // semitones from C-0 of the voice's last note, -1 if none yet
    int instrument; // instrument number the voice plays, -1 if none yet
} JtVoiceState;

typedef struct JtModule {
    uint8_t* data;
    uint32_t size;
    uint32_t load_address;
    uint32_t period_table;     // JT_PERIOD_COUNT words
    uint32_t pattern_table;    // addresses of the patterns
    uint32_t instrument_table; // addresses of the instruments
    uint32_t arpeggio_table;   // addresses of the arpeggio lists
    uint32_t song_table;
    uint32_t song_count;
    uint8_t first_track_pos[JT_VOICES]; // where a voice's track list restarts after the end mark
} JtModule;

// The instrument record. `sample` and `loop` are absolute addresses; a loop address of 0 makes the replayer loop over
// its own code.
typedef struct JtInstrument {
    uint16_t max_volume;
    uint16_t length_words;
    uint32_t sample;
    uint32_t loop;
    uint16_t loop_length_words;
    uint16_t attack_rate;
    uint16_t decay_rate;
    uint16_t sustain_volume;
    uint16_t release_rate;
    uint16_t vibrato_delay; // 0: no vibrato, otherwise it starts after this many ticks
    uint16_t vibrato_length;
    uint16_t vibrato_step;
    uint8_t arpeggio_list; // byte offset into the arpeggio table
    uint16_t release_tick; // the gate closes at this tick, 0 never
    uint16_t sweep;        // non-zero: the sample start moves one byte per two ticks
} JtInstrument;

typedef enum JtEnvelopeStage {
    JtEnvelopeStage_Attack = 0,
    JtEnvelopeStage_Decay = 1,
    JtEnvelopeStage_Sustain = 2,
} JtEnvelopeStage;

typedef struct JtVoice {
    uint8_t duration; // steps left of the current note, 0xFF when a new one is due
    uint8_t track_pos;
    uint8_t pattern_pos;
    uint8_t note; // note + transpose
    uint8_t slide_target;
    uint8_t transpose;
    uint8_t arpeggio_pos;
    uint8_t arpeggio_list;
    uint16_t slide_speed; // non-zero: the period slides towards the target note
    int16_t vibrato_count;
    bool vibrato_up;
    uint16_t period;
    uint16_t ticks; // since the note started
    uint16_t volume;
    JtEnvelopeStage stage;
    bool gate;
    uint32_t sweep_pos;
    uint32_t track; // offset of the track list
    JtInstrument instrument;
    int display_note;
    int display_instrument;
} JtVoice;

struct JtPlayer {
    JtModule module;
    uint8_t counter; // counts the ticks of a step
    uint8_t speed;
    bool playing;
    bool looped; // a track list reached its end mark in this tick
    JtVoice voices[JT_VOICES];
    NuadePaulaChannel paula[JT_VOICES];
};

// The eagleplayer's Check2 and the table scan of its InitPlayer: false when either would fail.
bool jt_probe(const uint8_t* data, size_t size);

// Copies the module and locates its tables; null when jt_probe fails.
JtPlayer* jt_create(const uint8_t* data, size_t size);
void jt_destroy(JtPlayer* player);

int jt_subsong_count(const JtPlayer* player);

// (Re)start a subsong (0-based) from its beginning; false when the subsong does not exist.
bool jt_start(JtPlayer* player, int subsong, NuadeVoices* voices);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
JtTickResult jt_tick(JtPlayer* player, NuadeVoices* voices);

// Ticks per sequencer step of a subsong (0 for a subsong that does not exist).
int jt_ticks_per_step(const JtPlayer* player, int subsong);

JtVoiceState jt_voice_state(const JtPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
JtPlayer* jt_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
