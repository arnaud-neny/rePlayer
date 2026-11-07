// SPDX-License-Identifier: MIT
// Maximum Effect player (Jonathan Scarcliffe / Alastair Dukes, 1993), translated from the UADE eagleplayer
// "Maximum_Effect" (Wanted Team). A song is two files: "MAX.name" holds the subsong records, the position lists and
// the patterns; "SMP.name" (or the shared "SMP.set") holds the instrument table and the sample data. Every voice runs
// its own position list.

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

#define MXE_VOICES 4

// The CIA clock: the timebase of MXE_TICK_LENGTH.
#define MXE_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MXE_TIMEBASE_HZ seconds.
#define MXE_TICK_LENGTH 14187

#define MXE_ARPEGGIO_NOTES 3
// The instrument number of a note is 7 bits.
#define MXE_MAX_INSTRUMENTS 128

typedef struct MxePlayer MxePlayer;

typedef enum MxeStatus {
    MxeStatus_Ok = 0,
    MxeStatus_InvalidData,
    MxeStatus_MissingSamples, // neither the "SMP." file of the song nor "SMP.set" could be read
    MxeStatus_OutOfMemory,
} MxeStatus;

typedef struct MxeCreateResult {
    MxePlayer* player;
    MxeStatus status;
} MxeCreateResult;

typedef struct MxeTickResult {
    bool looped; // every voice of the subsong has reached the end of its position list: the eagleplayer calls SongEnd
} MxeTickResult;

typedef struct MxeVoiceState {
    int period;     // the period of the last note started, 0 if none
    int instrument; // the instrument of that note, -1 if none
} MxeVoiceState;

// What a voice does on the ticks between two events.
typedef enum MxeEffect {
    MxeEffect_None = 0,
    MxeEffect_Vibrato,
    MxeEffect_VolumeSlide,
    MxeEffect_Portamento,
    MxeEffect_Arpeggio,
} MxeEffect;

// The four bytes the effects keep in the voice. The effects share them: a command rewrites the ones it uses and leaves
// the others (the continued vibrato keeps whatever phase byte the earlier command left).
typedef union MxeEffectParams {
    struct {
        int8_t volume_slide;
    } slide;
    struct {
        int8_t volume_slide; // added to the volume every tick
        uint8_t phase;       // the sine table position (bit 0 unused)
        uint8_t speed;       // added to the phase
        uint8_t depth_shift; // the sine is shifted left by this, then right by VIBRATO_SCALE
    } vibrato;
    struct {
        int8_t volume_slide;
        uint8_t target_offset; // the period table offset of the note to slide to
        int8_t speed;          // added to the period
    } portamento;
    struct {
        uint8_t note_offset[MXE_ARPEGGIO_NOTES]; // period table offsets
        uint8_t index;                           // the slot the next tick plays
    } arpeggio;
} MxeEffectParams;

typedef struct MxeVoice {
    NuadePaulaChannel paula;
    bool active;           // the interrupt runs the voice
    bool dma_start;        // a note started in this tick
    bool song_end_pending; // the voice has not reached the end of its list since the last SongEnd
    uint32_t list_start;
    uint16_t position;    // the next list entry, as a byte offset into the list
    uint16_t events_left; // of the current pattern
    uint32_t pattern_pos; // the next word of the pattern
    uint16_t timer;       // bit 15 set while the current event lasts
    MxeEffect effect;
    MxeEffectParams params;
    uint16_t base_period; // the period the vibrato swings around
    uint16_t period;
    uint16_t volume;
    const int8_t* repeat_lc; // the repeat of the current note's sample
    uint16_t repeat_words;
    int shown_period;
    int shown_instrument;
} MxeVoice;

typedef struct MxeModule {
    const uint8_t* bytes; // the whole file
    size_t size;
    uint32_t subsong_count;
    bool uses_instrument[MXE_MAX_INSTRUMENTS]; // set by mxe_module_validate
    uint32_t grid_ticks;                       // set by mxe_module_validate
} MxeModule;

typedef struct MxeSamples {
    uint8_t* bytes; // the sample file with the Paula tail (nuade_paula_padded_copy)
    size_t size;
    uint32_t instrument_count;
} MxeSamples;

struct MxePlayer {
    uint8_t* bytes; // the copy of the song file the module points into
    MxeModule module;
    MxeSamples samples;
    bool one_shot; // the subsong's voices stop at the end of their lists
    bool song_end_initial[MXE_VOICES];
    MxeVoice voices[MXE_VOICES];
};

// The eagleplayer's Check2 plus what its InitPlayer checks, plus the bounds this port needs: every list, pattern, note
// and command the subsongs can reach lies inside the file.
bool mxe_probe(const uint8_t* data, size_t size);

// Parses the song, loads its sample file from the song's directory and checks the instruments the song plays.
MxeCreateResult mxe_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void mxe_destroy(MxePlayer* player);

uint32_t mxe_subsong_count(const MxePlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mxe_start(MxePlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
MxeTickResult mxe_tick(MxePlayer* player, NuadeVoices* voices);

// Ticks per step of the voices' common grid: the greatest common divisor of the event lengths the song uses.
uint32_t mxe_grid_ticks(const MxePlayer* player);

MxeVoiceState mxe_voice_state(const MxePlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
MxePlayer* mxe_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
