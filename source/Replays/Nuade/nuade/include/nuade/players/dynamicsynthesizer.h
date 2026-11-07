// SPDX-License-Identifier: MIT
// Dynamic Synthesizer modules (Chris Huelsbeck's games: Hollywood Poker Pro, Star Ball, PTC), translated from the UADE
// eagleplayer "DynamicSynthesizer" (Wanted Team). A module is a game's sound code with its tables ("dns.name") and
// the samples are in "smp.name" next to it; the format and the replayers are described in players/dynamicsynthesizer/.

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

#define DNS_VOICES 4

// The CIA clock: the timebase of the tick lengths.
#define DNS_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz CIA timer, in 1 / DNS_TIMEBASE_HZ seconds.
#define DNS_TICK_LENGTH_50HZ 14187
// The eagleplayer sets the timer to 7094 for the modules that carry no relocation table (100 Hz).
#define DNS_TICK_LENGTH_100HZ 7094

#define DNS_TRACKS 6

typedef struct DnsPlayer DnsPlayer;

typedef enum DnsStatus {
    DnsStatus_Ok = 0,
    DnsStatus_InvalidData,
    DnsStatus_MissingSamples, // the "smp." file next to the song could not be read
    DnsStatus_OutOfMemory,
} DnsStatus;

typedef struct DnsCreateResult {
    DnsPlayer* player;
    DnsStatus status;
} DnsCreateResult;

typedef struct DnsTickResult {
    uint32_t length; // of the tick, in 1 / DNS_TIMEBASE_HZ seconds
    bool looped;     // the replayer reached the point where the eagleplayer calls SongEnd
} DnsTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the sample record number.
typedef struct DnsVoiceState {
    int note;
    int instrument;
} DnsVoiceState;

typedef struct DnsMemory {
    const uint8_t* data;
    uint32_t size;
} DnsMemory;

// The sample file, which the replayer writes to (it clears or sets the first long). The allocation comes from
// nuade_paula_padded_copy: a run that starts inside the file plays zeros past its end.
typedef struct DnsSamples {
    uint8_t* data;
    uint32_t size;
    uint8_t head[4]; // the first long as the file has it: the replayer overwrites it
} DnsSamples;

typedef enum DnsSequencer {
    DnsSequencer_FixedPatterns, // PTC: patterns of 48 words from a fixed address, no relocation table
    DnsSequencer_PatternTable,  // Star Ball: a table of pattern pointers
    DnsSequencer_Rows,          // Hollywood Poker Pro: 24 byte rows of six tracks, a pattern ends with 0xFFFF
} DnsSequencer;

typedef enum DnsEnvelopePhase {
    DnsEnvelopePhase_Idle = 0,
    DnsEnvelopePhase_Release = 1,
    DnsEnvelopePhase_Decay = 2, // to the sustain level, where it waits for the release
    DnsEnvelopePhase_Attack = 3,
} DnsEnvelopePhase;

// A voice's DMA sequence after a note: it starts the sample, then writes the repeat.
typedef enum DnsDmaState {
    DnsDmaState_Idle = 0,
    DnsDmaState_Repeat = 1, // the next step writes the repeat
    DnsDmaState_Start = 2,  // the next step starts DMA
    DnsDmaState_Delay = 3,  // one more tick before it starts
} DnsDmaState;

typedef enum DnsNoteOffGuard {
    DnsNoteOffGuard_None,
    DnsNoteOffGuard_Age, // a note-off for a voice that has already been released or is silent does nothing
    DnsNoteOffGuard_Map, // a note-off for a track without a voice does nothing
} DnsNoteOffGuard;

typedef struct DnsVersion {
    DnsSequencer sequencer;
    uint32_t tick_length; // 1 / DNS_TIMEBASE_HZ seconds
    // The order of the tick: the voices (volume, envelope, DMA) before or after the sequencer.
    bool voices_first;
    // The tick that loads a new position returns without updating the voices.
    bool new_position_skips_voices;
    // The song loops when the position equals the end after it has stepped, else when it passes the end.
    bool end_is_exact;
    // The first long of the samples is cleared at every note (it is the silent loop), else it is set to -1 at the
    // start.
    bool note_clears_sample_base;
    // The age of a voice counts in the DMA step and restarts there (every tick, every voice), else it counts in the
    // envelope step (voices that are not silent) and restarts at the note.
    bool age_counts_in_dma_step;
    uint16_t silent_age;     // the age of a voice whose volume is 0
    bool ages_start_silent;  // the start of the song sets the ages to 0xFFFF
    bool retrigger_releases; // a track that plays a note while its last one sounds releases it first
    DnsNoteOffGuard note_off_guard;
    bool note_unmaps_tracks;    // a note takes the voice from the track that has it
    DnsDmaState dma_note_state; // the DMA step state that a note sets: Start, or Delay for one tick more
    uint8_t volume_mask;        // the note's volume offset: (byte & mask) >> shift
    uint8_t volume_shift;
    uint16_t song_offset_mask; // the doubled song number is masked with this before it indexes the song table
} DnsVersion;

// A sample record (96 bytes): where the sample is, its envelope.
typedef struct DnsInstrument {
    uint32_t start;          // offset in the samples
    uint16_t start_words;    // length of the first run
    uint32_t loop;           // offset of the repeat
    uint16_t loop_words;     // length of the repeat
    uint16_t interval;       // ticks between envelope steps in the attack
    int16_t attack_step;     // added to the level every interval
    int16_t attack_from;     // start level, plus the note's volume offset
    int16_t attack_to;       // level that ends the attack, plus the note's volume offset
    uint16_t decay_interval; // ticks between the steps that follow the attack
    int16_t decay_step;
    uint16_t release_jumps; // 1: a release starts from the sustain level
    int16_t sustain;        // level that ends the decay
    uint16_t release_interval;
    int16_t release_step;
    uint16_t detune; // added to the period
} DnsInstrument;

// The tables the code addresses; all are offsets into the module.
typedef struct DnsTrackTables {
    uint32_t patterns;    // bytes: pattern number per position
    uint32_t note_adds;   // bytes
    uint32_t sample_adds; // bytes
} DnsTrackTables;

typedef struct DnsModule {
    DnsMemory memory;
    uint8_t* owned;
    DnsVersion version;
    uint32_t periods;     // 64 words
    uint32_t songs;       // 16 entries per field, see dns_read_song
    uint32_t instruments; // 96 bytes each
    uint32_t first_pattern;
    uint32_t pattern_table; // longs, one per pattern
    uint32_t pattern_count;
    uint32_t rows;
    DnsTrackTables tracks[DNS_TRACKS];
    uint32_t link_base;
    uint32_t subsongs;
} DnsModule;

typedef struct DnsVoice {
    NuadePaulaChannel paula;
    DnsDmaState dma_state;
    uint16_t age;
    bool held; // the track has not released the note
    DnsEnvelopePhase phase;
    int16_t level;
    int16_t step;
    int16_t target;
    uint16_t counter; // ticks to the next step
    uint16_t counter_reload;
    DnsInstrument instrument;
    int display_note;
    int display_instrument;
} DnsVoice;

typedef struct DnsTrack {
    uint32_t pattern; // offset of the pattern, 0 = none
    uint8_t note_add;
    uint8_t sample_add;
    bool muted;     // the song silences the track
    bool sounding;  // it has played a note that it has not released
    uint16_t voice; // the voice of its last note, or DNS_NO_VOICE
} DnsTrack;

struct DnsPlayer {
    DnsModule module;
    DnsSamples samples;
    DnsVoice voices[DNS_VOICES];
    DnsTrack tracks[DNS_TRACKS];
    bool playing;
    bool voices_active; // a note has played
    uint16_t beat_counter;
    uint16_t speed;
    uint16_t position; // of the song: the next position to load
    uint16_t loop_position;
    uint16_t end_position;
    uint16_t step; // byte offset in the patterns
    uint16_t step_limit;
    bool pattern_ended;      // rows version: a track saw the last step of its pattern
    uint8_t allocation_turn; // the voice where the next note's search starts
    bool looped;
};

// The eagleplayer's check (five `bra.w` at the start) plus the code scan of its InitPlayer; the file name must start
// with "dns." because the sample file's name is made from it. True for a module whose replayer is a known version.
bool dns_probe(const uint8_t* data, size_t size, const char* filename);

// Parses the module (the eagleplayer's InitPlayer) and loads the "smp." sample file.
DnsCreateResult dns_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void dns_destroy(DnsPlayer* player);

int dns_subsong_count(const DnsPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool dns_start(DnsPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
DnsTickResult dns_tick(DnsPlayer* player, NuadeVoices* voices);

// Ticks one sequencer step spans (the song's speed); 0 for a bad subsong.
uint32_t dns_grid_ticks(const DnsPlayer* player, int subsong);
DnsVoiceState dns_voice_state(const DnsPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
DnsPlayer* dns_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
