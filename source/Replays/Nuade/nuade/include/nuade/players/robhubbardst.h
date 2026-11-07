// SPDX-License-Identifier: MIT
// Rob Hubbard ST player: the Atari ST tunes with their own replayer ("*.rho", prefix rho).

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

#define RHST_VOICES 4

// The CIA clock: the timebase of RHST_TICK_LENGTH.
#define RHST_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / RHST_TIMEBASE_HZ seconds.
#define RHST_TICK_LENGTH 14187

#define RHST_RECORD_COUNT 3 // the replayer plays three voices of the ST sound chip
#define RHST_RAW_VOICES RHST_RECORD_COUNT
#define RHST_CHANNELS RHST_VOICES

typedef struct RhstPlayer RhstPlayer;

typedef struct RhstTickResult {
    bool looped; // the replayer's SongEnd call: the longest track list wrapped
    bool ended;  // the stop command ran
} RhstTickResult;

struct RhstBuild;

// Module bytes the eagleplayer overwrites after it loaded the module: the replayer's stores to the audio registers
// become calls into the eagleplayer, and a few branches are redirected. The sample windows of the replayer read some of
// them.
typedef struct RhstPatch {
    uint16_t offset;
    uint8_t size;
    uint8_t bytes[18];
} RhstPatch;

typedef enum RhstOutput {
    RhstOutput_Idle,   // writes the registers while the song runs, silences the second channel when it does not
    RhstOutput_Gated,  // writes the registers while the song runs
    RhstOutput_Always, // writes the registers every tick; the second channel plays noise when the mixer says so
} RhstOutput;

typedef struct RhstBuild {
    uint64_t code_hash; // of the module bytes from 0x38 to `instruments` and from `commands` to `records`
    uint32_t periods;
    uint32_t instruments;
    uint32_t commands;
    uint32_t records;
    uint32_t data;
    uint32_t noise;
    uint16_t noise_words;
    RhstOutput output;
    bool plays_subsong;  // the init starts the subsong that was asked for; otherwise it always starts the first
    bool clears_records; // the init clears the voice records of the file
    const RhstPatch* patches;
    size_t patch_count;
} RhstBuild;

// The state of one voice. The module keeps it in a record behind the code; the third build's noise sample reaches the
// first record, so the module writes it back into its image (rhst_module_store_voice).
typedef struct RhstVoice {
    bool toggle;     // flips every tick
    bool percussion; // noise percussion
    bool slide;
    bool portamento;
    bool tie;      // the next note keeps the envelope running
    bool envelope; // the envelope is running
    bool attack;   // ... and is rising
    bool porta_up;
    uint32_t stream;
    uint32_t track_list;
    uint32_t track_pos; // of the next entry
    uint16_t slide_total;
    uint32_t env_start; // the semitone list
    uint32_t env_at;
    int16_t slide_step;
    uint16_t slide_delay;
    uint16_t step_count; // steps until the next command; counts down
    uint16_t step_length;
    uint16_t note;
    uint8_t volume;
    uint8_t attack_init;
    uint8_t decay_init;
    uint8_t rate;
    uint8_t phase_init;
    uint8_t phase_left;
    uint8_t start_volume;
    uint16_t vib_depth;
    uint16_t vib_step;
    uint16_t vib_pos;
    bool vib_on;
    bool vib_half;
    bool vib_rising;
    bool noise_trigger;
    uint8_t mixer_mask; // the bits of the chip's mixer register this voice owns
} RhstVoice;

typedef struct RhstModule {
    const struct RhstBuild* build;
    uint8_t* image; // the module in chip memory, patched; padded so that any sample window is readable
    uint32_t size;
    uint32_t song_count;
    uint32_t periods; // period table, 2 bytes per semitone
    uint32_t data;    // the song table, the track lists and the streams
    uint32_t noise;   // offset of the noise sample
    uint16_t noise_words;
    RhstOutput output;
    bool noise_reads_records; // the noise sample covers the voice records: they are kept up to date in the image
    uint8_t mixer_mask[RHST_RECORD_COUNT];
    uint32_t* list_counts; // entries of every track list: song * RHST_RECORD_COUNT + voice
} RhstModule;

struct RhstPlayer {
    RhstModule* module;
    RhstVoice voices[RHST_RAW_VOICES];
    NuadePaulaChannel paula[RHST_CHANNELS];
    bool active;
    bool song_ended;
    bool ended;
    uint32_t song;
    uint32_t longest_list;
    uint16_t tempo;
    uint16_t tempo_count;
    uint8_t noise_freq; // noise period the song set
    uint8_t noise_work; // ... as this tick computes it
    uint8_t noise_out;
    uint8_t mixer_shadow;
    uint8_t mixer_out;
    uint16_t out_period[RHST_RAW_VOICES];
    uint8_t out_volume[RHST_RAW_VOICES];
    bool fourth_written; // third build: the fourth channel is written from the second tick
};

// True when the eagleplayer plays the module and this port reproduces it: the eagleplayer's checks, a replayer build
// the port knows, and sequence data the port models.
bool rhst_probe(const uint8_t* data, size_t size);

// null when rhst_probe is false.
RhstPlayer* rhst_create(const uint8_t* data, size_t size);
void rhst_destroy(RhstPlayer* player);

int rhst_subsong_count(const RhstPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool rhst_start(RhstPlayer* player, int subsong, NuadeVoices* voices);

RhstTickResult rhst_tick(RhstPlayer* player, NuadeVoices* voices);

// The player's state behind a song it opened; 0 when the song is another player's.
RhstPlayer* rhst_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
