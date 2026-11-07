// SPDX-License-Identifier: MIT
// Synth Dream player (Laurens Tummers' music driver for the games Monster Business and No Buddies Land, "sdr."
// modules), translated from the UADE eagleplayer "SynthDream". A module holds the instrument records, the pitch and
// volume tables and four voices' tracks; the sounds are either samples from an external "smp." file or 16-byte
// waveforms the replayer builds from pulse presets. Every voice runs its own command stream, so the notes of the four
// voices have no common row grid.

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

#define SDR_VOICES 4

// The CIA clock: the timebase of SDR_TICK_LENGTH.
#define SDR_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SDR_TIMEBASE_HZ seconds.
#define SDR_TICK_LENGTH 14187

#define SDR_WAVE_BYTES 16 // a synthesised waveform: 8 words

typedef struct SdrPlayer SdrPlayer;

typedef enum SdrStatus {
    SdrStatus_Ok = 0,
    SdrStatus_InvalidData,
    SdrStatus_MissingSamples, // neither the song's "smp." file nor "smp.set" could be read
    SdrStatus_Unmodelled,     // the song reads memory or runs commands the port does not model
    SdrStatus_OutOfMemory,
} SdrStatus;

typedef struct SdrCreateResult {
    SdrPlayer* player; // null unless status is Ok
    SdrStatus status;
} SdrCreateResult;

typedef struct SdrTickResult {
    bool looped; // the eagleplayer's SongEnd was called during the tick
} SdrTickResult;

typedef struct SdrVoiceState {
    int note;       // the note the voice last started, -1 if none yet
    int instrument; // its instrument number, -1 if none yet
} SdrVoiceState;

typedef struct SdrMemory {
    uint8_t* module;
    uint32_t module_size;
    uint8_t* samples;
    uint32_t sample_size;
    uint32_t sample_base; // the address of the sample file
    bool samples_unknown; // the probe has no sample file: the memory from sample_base on may hold anything
    bool stray;
} SdrMemory;

// The three table offsets at the start of the module.
typedef struct SdrHeader {
    uint32_t instruments;     // 32-byte instrument records
    uint32_t volume_switches; // longs: volume envelope offsets a note can switch to
    uint32_t pitch_switches;  // longs: pitch envelope offsets a note can switch to
} SdrHeader;

// Where an instrument's sample or waveform is: offsets in the sample file.
typedef struct SdrSampleRef {
    uint32_t start;
    uint16_t length_words;
    uint32_t loop_start;
    uint16_t loop_length_words;
} SdrSampleRef;

// An instrument record: the sound, and the tables its notes step through. A table offset of 0 means none.
typedef struct SdrInstrument {
    bool muted;            // junk record (see sdr_read_instrument): the note plays nothing and the fields are zero
    bool fixed_pitch;      // the track's transposition does not apply
    uint16_t detune;       // byte offset into the pitch ratio tables (0: none)
    uint32_t volume_table; // one volume byte per tick
    uint32_t pitch_table;  // one signed pitch word per tick
    uint32_t arpeggio;     // one note offset byte per tick
    uint32_t waveform;     // synthesised waveform: one (preset, subtraction) pair per tick; else the sample
    SdrSampleRef sample;
} SdrInstrument;

// One of the four tables a note steps through. Entries are [length][repeats] followed by `length` elements; an entry
// that starts with 0xFF restarts the table.
typedef struct SdrCursor {
    uint32_t at;     // next element
    uint8_t left;    // elements left in this pass
    uint8_t repeats; // passes left after this one
    uint8_t length;  // elements of the entry
} SdrCursor;

// A table switch a note asks for: after `delay` ticks the cursor continues at `target`.
typedef struct SdrSwitch {
    uint32_t target;
    uint16_t delay;
} SdrSwitch;

typedef struct SdrVoice {
    NuadePaulaChannel paula;
    bool ended;
    bool hold;      // the sample keeps playing after the note's last tick
    uint32_t loops; // times the track started over

    uint32_t track_start;
    uint32_t track_next;
    uint32_t commands_start; // the current entry's stream, for its repeats
    uint32_t commands;       // offset of the next command
    uint8_t transpose;
    uint8_t repeat;
    uint8_t substitute_of;
    uint8_t substitute_by;
    uint8_t volume_cut; // track entry

    uint16_t left;     // ticks of the note still to run
    uint16_t duration; // ticks of the note
    uint16_t note;
    uint16_t note_cut; // the low byte is subtracted from the note's volume
    bool rest;         // the note is a rest: DMA off

    SdrInstrument instrument;
    SdrCursor volume_cursor;
    SdrCursor pitch_cursor;
    SdrCursor arpeggio_cursor;
    SdrCursor waveform_cursor;
    SdrSwitch volume_switch;
    SdrSwitch pitch_switch;
    uint8_t arpeggio_offset;
    uint16_t slide;       // accumulated slide ratio (0 = none, 0x2000 = 1.0)
    uint16_t slide_ratio; // per tick
    uint16_t slide_delay;

    uint16_t period;
    uint8_t volume;
    int display_note;
    int display_instrument;
} SdrVoice;

struct SdrPlayer {
    SdrMemory memory;
    SdrHeader header;
    int subsong_count;

    SdrVoice voices[SDR_VOICES];
    int8_t waveforms[SDR_VOICES * SDR_WAVE_BYTES]; // the player's 0x40 bytes of BSS: one buffer per voice

    bool voice_pending[SDR_VOICES]; // voices that have not reached the end of their track since the last SongEnd
    bool voice_pending_reload[SDR_VOICES];
    bool song_ended;
    bool unmodelled; // a command the replayer cannot run (see run_commands)
    uint64_t work;   // steps taken, for the proof's budget
};

// Does the file name start with "sdr." (any case), as the eagleplayer's prefix?
bool sdr_name_matches(const char* filename);

// What open accepts, as far as the module alone decides: Check2, the InitPlayer size check and the proof that the song
// only reads memory the port models (the sample file is not known here, so memory behind the module may hold anything).
bool sdr_probe(const uint8_t* data, size_t size);

// Copies the module and loads its sample file (ExtLoad) through `io`; `path` is the song's url.
SdrCreateResult sdr_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void sdr_destroy(SdrPlayer* player);

int sdr_subsong_count(const SdrPlayer* player);

// (Re)start a subsong (0-based) from its beginning; false when the subsong does not exist.
bool sdr_start(SdrPlayer* player, int subsong);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
SdrTickResult sdr_tick(SdrPlayer* player, NuadeVoices* voices);

SdrVoiceState sdr_voice_state(const SdrPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SdrPlayer* sdr_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
