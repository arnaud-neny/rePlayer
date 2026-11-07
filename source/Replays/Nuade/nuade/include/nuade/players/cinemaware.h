// SPDX-License-Identifier: MIT
// Cinemaware player (the music of the game "Wings", 1990), translated from the UADE eagleplayer "Cinemaware player
// module V1.0" (Wanted Team). A module is an instrument bank ("IBLK": envelopes and the names of up to three IFF 8SVX
// samples per instrument, which are external files in an Instruments/ directory) and a MIDI-like event stream
// ("ASEQ"); the eagleplayer holds the replayer.

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

#define CW_VOICES 4
#define CW_PROGRAMS 128

// The CIA clock: the timebase of CW_TICK_LENGTH.
#define CW_TIMEBASE_HZ 709379
// One player tick: InitSound lowers DeliTracker's 50 Hz CIA timer (14187) to 5/6 of its value, so the replayer runs at
// 60 Hz. In 1 / CW_TIMEBASE_HZ seconds.
#define CW_TICK_LENGTH 11822

#define CW_CHANNEL_COUNT 16
#define CW_ZONES_PER_INSTRUMENT 3
#define CW_ENVELOPE_STAGES 8
#define CW_ZONE_NAME_SIZE 6

typedef struct CwPlayer CwPlayer;

typedef enum CwStatus {
    CwStatus_Ok = 0,
    CwStatus_InvalidData,
    CwStatus_MissingSample, // a named sample has no file in Instruments/ (UADE refuses the song too)
    CwStatus_OutOfMemory,
} CwStatus;

typedef struct CwCreateResult {
    CwPlayer* player; // 0 on failure
    CwStatus status;
    char missing_sample[16]; // the sample name, for CwStatus_MissingSample
} CwCreateResult;

typedef struct CwTickResult {
    bool looped; // the end event ran (the eagleplayer's SongEnd); the song restarts after a pause
} CwTickResult;

typedef struct CwVoiceState {
    int note;       // key of the last note started, -1 if none
    int instrument; // program number of the last note started, -1 if none
} CwVoiceState;

// One of the three sample zones of an instrument: the keys it covers and the sample it plays them with.
typedef struct CwZone {
    char name[CW_ZONE_NAME_SIZE + 1]; // the sample file in Instruments/
    bool has_sample;                  // the module names a sample (the replayer tests its pointer field)
    uint8_t key_low;
    uint8_t key_high;
    uint16_t period; // the period of key 60
    int32_t start;   // byte offsets in the sample: the first pass plays start..end,
    int32_t loop;    // then the repeat plays loop..end; a negative loop is a one-shot
    int32_t end;
    int8_t* data;  // BODY of the sample file and NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes (nuade_paula_padded_copy),
                   // owned; 0 until it is loaded
    uint32_t size; // of the BODY
} CwZone;

typedef struct CwInstrument {
    uint8_t program;
    int8_t transpose;       // semitones
    uint8_t velocity_boost; // added to the note velocity
    uint8_t vibrato_max;    // the vibrato swings between 0 and this
    uint8_t vibrato_step;   // and moves this much per tick
    uint8_t bend_range;     // scale of the pitch bend; BEND_FIXED_PITCH also fixes the key
    uint8_t pitch_release;  // pitch envelope stage a note off jumps to
    uint8_t pitch_last;     // while the key is down, the pitch envelope loops back after this stage
    uint8_t pitch_level[CW_ENVELOPE_STAGES];
    uint8_t pitch_rate[CW_ENVELOPE_STAGES];
    uint8_t volume_release; // volume envelope stage a note off jumps to
    uint8_t volume_last;    // while the key is down, the volume envelope loops back after this stage
    uint8_t volume_level[CW_ENVELOPE_STAGES];
    uint8_t volume_rate[CW_ENVELOPE_STAGES];
    CwZone zones[CW_ZONES_PER_INSTRUMENT];
} CwInstrument;

typedef struct CwModule {
    const uint8_t* data;
    size_t size;
    int instrument_count;
    CwInstrument instruments[CW_PROGRAMS];   // in file order
    int16_t block_of_program[CW_PROGRAMS];   // index into instruments, -1 if undefined; the last block wins
    uint8_t channel_voice[CW_CHANNEL_COUNT]; // >= CW_VOICES: any voice
    size_t events;                           // offset of the first event
} CwModule;

// A MIDI channel.
typedef struct CwChannel {
    uint8_t program;
    uint8_t bend;
    uint8_t volume;
    bool pedal;
} CwChannel;

typedef struct CwVoice {
    int8_t channel; // the MIDI channel it plays, NO_CHANNEL when free
    int8_t key;
    int8_t velocity;
    uint8_t program;
    bool played; // a note was started on the voice
    uint8_t age; // counts down from NOTE_AGE_START while the voice plays
    bool key_down;
    bool pedal; // the pedal was down at the note start (and the key may be up)
    uint16_t period_scale;
    int8_t volume_stage;
    int8_t pitch_stage;
    uint16_t volume_level;
    uint16_t pitch_level;
    int16_t vibrato;
    int16_t vibrato_step;
    uint8_t latch_ticks; // ticks until the repeat registers are written, 0 once they are
    const int8_t* loop_lc;
    uint16_t loop_len_words;

    NuadePaulaChannel paula;
} CwVoice;

typedef struct CwSequencer {
    size_t position; // offset of the next event
    int16_t delay;   // ticks left before it
    uint8_t pause;   // ticks left of the pause after the end event
} CwSequencer;

struct CwPlayer {
    CwModule module;
    CwChannel channels[CW_CHANNEL_COUNT];
    CwVoice voices[CW_VOICES];
    CwSequencer sequencer;
    bool looped;
};

// The eagleplayer's Check2: "IBLK", an instrument count of 1..128, and "ASEQ" after the instrument blocks.
bool cw_probe(const uint8_t* data, size_t size);

// Loads the module and its samples, which are external: `<directory of path>/Instruments/<sample name>`, any case.
CwCreateResult cw_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void cw_destroy(CwPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool cw_start(CwPlayer* player, NuadeVoices* voices);

CwTickResult cw_tick(CwPlayer* player, NuadeVoices* voices);

// The name of the first sample of an instrument (program 0..CW_PROGRAMS-1), "" when the program is not defined.
const char* cw_instrument_name(const CwPlayer* player, int instrument);

CwVoiceState cw_voice_state(const CwPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
CwPlayer* cw_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
