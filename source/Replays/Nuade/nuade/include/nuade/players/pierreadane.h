// SPDX-License-Identifier: MIT
// Pierre Adane Packer player, translated from the UADE eagleplayer "PierreAdane" (Wanted Team). A song is a "pap"
// file (four lists of track offsets, an instrument table, the tracks) whose samples are in a "smp.set" file (or
// "smp.<name>" for a song called "pap.<name>") in the same directory; the replayer is in the eagleplayer.

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

#define PA_VOICES 4

// The CIA clock: the timebase of PA_TICK_LENGTH.
#define PA_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / PA_TIMEBASE_HZ seconds.
#define PA_TICK_LENGTH 14187

#define PA_MAX_INSTRUMENTS 15 // the sample pointer table of the replayer holds 15 entries

typedef struct PaPlayer PaPlayer;

typedef enum PaStatus {
    PaStatus_Ok = 0,
    PaStatus_InvalidData,
    PaStatus_MissingSamples, // no sample set next to the song, or it is shorter than the instruments need
    PaStatus_OutOfMemory,
} PaStatus;

typedef struct PaCreateResult {
    PaPlayer* player;
    PaStatus status;
} PaCreateResult;

typedef struct PaTickResult {
    bool looped; // the first voice's track list ended: the eagleplayer calls SongEnd and the song restarts
    bool ended;  // the song reached something the replayer does with memory this port does not model
} PaTickResult;

// What voice (0-based) last started: the period of its note (0 if none) and the instrument number.
typedef struct PaVoiceState {
    int period;
    int instrument;
} PaVoiceState;

typedef enum PaEffect {
    PaEffect_Arpeggio = 0x0,  // param: high nibble and low nibble are semitones above the note
    PaEffect_SlideUp = 0x1,   // param & 15: period decrease per tick
    PaEffect_SlideDown = 0x2, // param & 15: period increase per tick
    PaEffect_Volume = 0xC,    // param: volume
    PaEffect_Speed = 0xF,     // param & 15: ticks per row (0 keeps the speed)
    // The other effects do nothing.
} PaEffect;

typedef struct PaNote {
    uint16_t period; // 0 = no new note
    uint8_t instrument;
    PaEffect effect;
    uint8_t param;
} PaNote;

typedef struct PaModule {
    const uint8_t* data;
    uint32_t size;
    uint32_t instrument_count; // records before the 0xFF terminator
} PaModule;

typedef struct PaSamples {
    uint8_t* data; // nuade_paula_padded_copy of the sample set
    uint32_t size;
    uint32_t start[PA_MAX_INSTRUMENTS]; // offset of the sample of instrument n + 1
} PaSamples;

typedef struct PaVoice {
    PaNote note;                // the entry of this row, all zero between entries
    uint32_t sample;            // sample set offset the next note starts at
    bool sample_known;          // the instrument has a sample in the set
    uint16_t length_words;      // AUDxLEN of the next note
    uint16_t loop_length_words; // of the instrument; 1 = no loop, the repeat is set one tick after the start
    uint16_t base_period;       // the period of the last note, arpeggio is relative to it
    uint16_t period;            // the period now, after slides
    int16_t rows_left;          // rows the current entry still lasts, -1 = read the next entry
    uint16_t row_length;        // rows per entry (the marker's length - 1)
    uint16_t track;             // position in the song file
    uint8_t instrument;         // instrument of the last note
    bool start_dma;             // a note of this row started: the DMA goes on after the raster wait
    NuadePaulaChannel paula;
} PaVoice;

struct PaPlayer {
    uint8_t* module_copy; // the song file the module points into
    PaModule module;
    PaSamples samples;
    uint16_t list_position; // byte position in the track lists of the next track
    uint16_t row_in_track;  // 0..63
    uint16_t tick;          // 0 at the first tick of a row
    uint16_t speed;         // ticks per row
    bool unmodeled;         // something the replayer does with memory outside this model happened
    bool looped;
    PaVoice voices[PA_VOICES];
};

// The eagleplayer's Check2 on the song file, plus what this port needs: an instrument table the sample pointer table
// can hold.
bool pa_probe(const uint8_t* data, size_t size);

// Parses the song (the eagleplayer's InitPlayer) and loads its sample set through `io`.
PaCreateResult pa_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void pa_destroy(PaPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool pa_start(PaPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
PaTickResult pa_tick(PaPlayer* player, NuadeVoices* voices);
PaVoiceState pa_voice_state(const PaPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PaPlayer* pa_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
