// SPDX-License-Identifier: MIT
// UFO (MicroProse, "UFO: Enemy Unknown" AGA / CD32), translated from the UADE eagleplayer "UFO" (Wanted Team).
// The module formats and the replayer are described in players/ufo/.

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

#define UFO_VOICES 4

// The CIA clock: the timebase of UFO_TICK_LENGTH.
#define UFO_TIMEBASE_HZ 709379
// One player tick: the eagleplayer sets DeliTracker's 50 Hz timer (14187) to 5/6 of it, 60 Hz.
#define UFO_TICK_LENGTH 11822

#define UFO_CHANNELS 16
#define UFO_INSTRUMENTS 40
#define UFO_MAX_BLOCKS 80

typedef struct UfoPlayer UfoPlayer;

typedef enum UfoStatus {
    UfoStatus_Ok = 0,
    UfoStatus_InvalidData,
    UfoStatus_MissingBank, // neither "<name>.BANK" nor "SMP.set" next to the song could be read
    UfoStatus_OutOfMemory,
} UfoStatus;

typedef struct UfoCreateResult {
    UfoPlayer* player;
    UfoStatus status;
} UfoCreateResult;

typedef struct UfoTickResult {
    bool looped; // the song reached the loop command, where the eagleplayer calls SongEnd and the song restarts
    bool ended;  // the song reached its stop command: every voice is silent from here
} UfoTickResult;

// What a voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct UfoVoiceState {
    int note;
    int instrument;
} UfoVoiceState;

// An instrument of the song file (INST chunk): the sample follows the chunk header.
typedef struct UfoSongInstrument {
    bool present;
    uint32_t sample_offset; // in the song file
    uint32_t length;        // bytes
    uint32_t loop_offset;   // 0: no loop
    uint8_t volume;
} UfoSongInstrument;

// A playable instrument (the replayer's 22 byte sample record): the sample is in the song file or the sample bank.
typedef struct UfoInstrument {
    const int8_t* start; // null: no sample, a note on it only silences the voice
    const int8_t* loop_start;
    const int8_t* end;
    bool loops;
    uint8_t volume;
} UfoInstrument;

typedef struct UfoSong {
    const uint8_t* data; // the song file
    uint32_t size;
    uint32_t channel_start[UFO_CHANNELS]; // offset of the stream after its first delay; 0: the channel is unused
    uint16_t channel_delay[UFO_CHANNELS]; // that first delay
    uint32_t blocks[UFO_MAX_BLOCKS];      // offsets of the BLCK chunks' streams
    uint32_t block_count;
    uint8_t bpm; // the BPM chunk scaled to sequencer rounds per second
    UfoSongInstrument song_instruments[UFO_INSTRUMENTS];
    UfoInstrument instruments[UFO_INSTRUMENTS]; // bank and song instruments; empty until the bank is loaded
    const int8_t* silence;                      // the bank's cleared first long word, repeated by notes without loop
} UfoSong;

// The sequencer (the replayer's variables bpm, count, chan tables, v1b28/v1ba8 with their v1b68/v1bc8 copies, v1d2a).
typedef struct UfoSequencer {
    int32_t count;
    uint32_t position[UFO_CHANNELS]; // offsets of the next event in the song file; 0: unused
    uint32_t saved_position[UFO_CHANNELS];
    uint16_t delay[UFO_CHANNELS]; // rounds until the channel reads its next event
    uint16_t saved_delay[UFO_CHANNELS];
    uint8_t command[UFO_CHANNELS]; // last command byte of the channel (running status)
    uint8_t bpm;
    bool playing;
} UfoSequencer;

// Per logical channel (the replayer's 10 byte records at v170c).
typedef struct UfoChannel {
    uint8_t instrument;
    uint8_t volume_scale; // velocity scale, 0x7F at start
    int8_t bend;          // period bend in slide steps
    uint16_t base_period; // of the channel's last note
    uint32_t return_position;
} UfoChannel;

// Per Paula voice (the replayer's 14 byte records at v16d4), with the voice's Paula registers.
typedef struct UfoVoice {
    NuadePaulaChannel paula;
    uint16_t slide_down; // period step of one bend unit towards the next lower note, and towards the next higher
    uint16_t slide_up;
    uint8_t instrument_volume;
    int8_t target;    // the envelope level moves half the way to it every tick
    int8_t level;     // current volume
    uint8_t ceiling;  // the attack stops here
    uint16_t channel; // the logical channel and the note of the last note on
    uint16_t note_index;
    int display_note;
    int display_instrument;
} UfoVoice;

typedef struct UfoState {
    UfoSequencer seq;
    UfoChannel channels[UFO_CHANNELS];
    UfoVoice voices[UFO_VOICES];
    uint64_t work; // work done so far, which the proof run limits
    bool fault;    // the replayer would have read outside the song, or hung: only the proof run ever sees this
    bool looped;
    bool ended;
} UfoState;

struct UfoPlayer {
    UfoSong song;
    UfoState state;
    uint8_t* module_memory; // the song file as Paula sees it (song instruments)
    uint8_t* bank_memory;   // the sample bank
};

// The eagleplayer's check (FORM, DDAT, BODY, CHAN) and the module walk the open does: the chunks, and a dry run of the
// sequencer that proves every read it makes stays inside the file. The sample bank is not needed for either.
bool ufo_probe(const uint8_t* data, size_t size);

// Parses the song and loads its sample bank (the eagleplayer's InitPlayer and ExtLoad).
UfoCreateResult ufo_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void ufo_destroy(UfoPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ufo_start(UfoPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
UfoTickResult ufo_tick(UfoPlayer* player, NuadeVoices* voices);
UfoVoiceState ufo_voice_state(const UfoPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
UfoPlayer* ufo_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
