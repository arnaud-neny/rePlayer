// SPDX-License-Identifier: MIT
// Thomas Hermann player (1989), translated from the UADE eagleplayer "ThomasHermann" (Wanted Team). The module
// ("thm." prefix) holds four pattern lists, the instrument records and the song header; the samples are one "smp."
// file next to it. The replayer runs from two CIA timers (the sequencer at the module's rate, the envelope and
// portamento handler at 25.8 Hz) and chains the sample blocks from the channels' audio interrupts.

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

#define THM_VOICES 4

// The CIA clock: the timebase of the tick lengths.
#define THM_TIMEBASE_HZ 709379

#define THM_MAX_INSTRUMENTS 31

typedef struct ThmPlayer ThmPlayer;

typedef enum ThmStatus {
    ThmStatus_Ok = 0,
    ThmStatus_InvalidData,
    ThmStatus_MissingSamples, // the "smp." file next to the song could not be read
    ThmStatus_OutOfMemory,
} ThmStatus;

typedef struct ThmCreateResult {
    ThmPlayer* player; // 0 on failure
    ThmStatus status;
} ThmCreateResult;

typedef struct ThmTickResult {
    uint32_t length; // until the next timer interrupt or audio block start, in 1 / THM_TIMEBASE_HZ seconds
    bool looped;     // the sequencer passed the song's last position (the eagleplayer's SongEnd call)
} ThmTickResult;

// What voice (0-based) plays: the period of its last note (0 if none yet) and its instrument.
typedef struct ThmVoiceState {
    uint32_t period;
    int instrument;
} ThmVoiceState;

// The song header at the start of the module.
typedef struct ThmHeader {
    uint8_t loop_position;
    uint8_t last_position;
    uint8_t position;
    uint8_t instrument_count;
    uint32_t instrument_table; // pointer, relocated by `base`
    uint8_t last_row;          // of a pattern
    uint16_t timer;            // of the sequencer interrupt
    uint8_t speed;
    uint32_t base;              // the address the module's pointers were assembled for
    bool voice_off[THM_VOICES]; // the voice starts no notes
} ThmHeader;

typedef struct ThmModule {
    uint8_t* chip;        // the module, the sample file, then zeros (nuade_paula_padded_copy)
    uint32_t module_size; // the sample file starts here
    uint32_t chip_size;   // module + sample file
    ThmHeader header;
    uint32_t subsong_count;
    uint32_t sample[THM_MAX_INSTRUMENTS + 1]; // chip offset of each instrument's sample data
} ThmModule;

typedef struct ThmRow {
    uint8_t note;
    uint8_t instrument;
    bool slide;   // the period slides to the note's instead of jumping
    bool no_stop; // the voice is not stopped the tick before the row
} ThmRow;

// The envelope segments: the attack, three more with the instrument's levels, then the fall to 0 of a release.
typedef enum ThmStage {
    ThmStage_Attack = 0,
    ThmStage_First,
    ThmStage_Second,
    ThmStage_Third,
    ThmStage_Release,
    ThmStage_Done,
} ThmStage;

typedef struct ThmChannel {
    NuadePaulaChannel paula;
    bool irq_enabled; // INTENA bit of the channel
    bool irq_pending; // INTREQ bit
    uint32_t wraps_seen;
    uint8_t irq_count;    // block starts the interrupt has seen of the current note
    uint8_t loop_flags;   // the note's loop flags
    uint32_t loop_sample; // the repeat the interrupt writes
    uint16_t loop_words;
    uint8_t envelope_flags; // the loop flags while the envelope handler works on the voice; 0 when it must not
    bool release;
    uint16_t level;      // high byte: the envelope level, 0x40 = full volume
    int16_t slope;       // added to the level at every envelope step
    ThmStage stage;      // envelope segment
    uint8_t note_volume; // the instrument's volume, scaled by the level
    uint8_t instrument;  // the voice's current instrument
    bool sliding;
    uint16_t period;        // the sliding period
    uint16_t target_period; // the period a sliding note moves to
    uint16_t note_period;   // last note's period, for the display
    ThmRow row;             // the row read ahead
} ThmChannel;

struct ThmPlayer {
    ThmModule* module;
    ThmChannel channel[THM_VOICES];
    uint8_t loop_position;
    uint8_t last_position;
    uint8_t position;
    uint8_t speed;
    int8_t tick_counter; // 0 reads the rows ahead, negative runs them
    uint8_t row;
    uint8_t envelope_counter;
    uint32_t sequencer_period;
    uint64_t time; // of the interrupt that runs now
    uint64_t next_sequencer;
    uint64_t next_envelope;
    uint32_t sample_rate; // of the mixer
    bool looped;
};

// The eagleplayer's check (Check2) and its file name rule: only "thm." names load the sample file.
bool thm_probe(const uint8_t* data, size_t size, const char* filename);

// Parses the module (the eagleplayer's InitPlayer), loads the sample file and proves that every song the module offers
// plays only what the port models.
ThmCreateResult thm_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void thm_destroy(ThmPlayer* player);

uint32_t thm_subsong_count(const ThmPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool thm_start(ThmPlayer* player, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices);

// Run what is due now: the timer interrupt(s) and the audio interrupts of the block starts played since the last
// tick, then hand the Paula registers they wrote to the voices. A tick is also due at the next block start of a channel
// whose audio interrupt can still act.
ThmTickResult thm_tick(ThmPlayer* player, NuadeVoices* voices);
ThmVoiceState thm_voice_state(const ThmPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
ThmPlayer* thm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
