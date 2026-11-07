// SPDX-License-Identifier: MIT
// Special FX sound driver (.jd): the modules of Ocean's Special FX group carry their own 68k driver; this is a
// translation of it.

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

#define JD_MAX_VOICES 4

// The CIA clock: the timebase of JD_TICK_LENGTH.
#define JD_TIMEBASE_HZ 709379
// One driver tick: DeliTracker's default 50 Hz interrupt, in 1 / JD_TIMEBASE_HZ seconds.
#define JD_TICK_LENGTH 14187

typedef struct JdPlayer JdPlayer;

typedef struct JdTickResult {
    bool looped; // a voice's track list started over during this tick
    bool ended;  // every voice stopped, or the subsong has no voice: the driver does nothing more
} JdTickResult;

// What voice (0-based) last started: the note in semitones from C-0 (-1 if none yet) and the instrument (-1 if none).
typedef struct JdVoiceState {
    int note;
    int instrument;
} JdVoiceState;

typedef struct JdModule {
    uint8_t* data; // nuade_paula_padded_copy: every AUDxLC into it lies in the first `size` bytes
    size_t size;
    uint32_t base; // absolute address of file offset 0 in the original link
} JdModule;

// How the setup routine finds the 8SVX forms of the instruments (four builds of it).
typedef enum JdSetupKind {
    JdSetupKind_RelativeList = 0, // longs: offset of the form from the entry, then base note and flag words
    JdSetupKind_FormStream,       // forms follow each other; the base notes are in the instrument table already
    JdSetupKind_PointerList,      // words: marker, base note, flag; the forms' addresses are in a second list
    JdSetupKind_WordList,         // words: offset of the form from the entry, base note, flag
} JdSetupKind;

// What differs between the builds of the driver.
typedef struct JdVersion {
    int voices;
    bool zero_wait_halts;    // a channel whose wait counter is 0 stays silent (else the counter wraps)
    bool activity_overwrite; // the first channel stores the activity mask, the others or into it
    bool volume_half;        // AUDxVOL is the volume shifted right once
    int vibrato_shift;       // vibrato depth: period difference >> this
    bool track_pattern_byte; // track list command 0x84 gives a pattern number as the next byte (song start only)
    // Table entries hold the offset of their data from the start of the table (else from the entry itself)
    bool patterns_start_relative;
    bool patterns_play_relative;
    bool pitch_seq_relative;
    bool envelope_relative;
    JdSetupKind setup_kind;
} JdVersion;

// Addresses (file offsets) of the driver's tables.
typedef struct JdTables {
    uint32_t songs;    // 8 bytes per song: a word per voice, the offset of its track list from that word
    uint32_t patterns; // words: offset of pattern n from its entry
    uint32_t pitch_seq;
    uint32_t envelope;
    uint32_t period;            // periods, indexed by note - base note of the instrument
    uint32_t instruments;       // instrument records; the first one the setup routine fills
    uint32_t instrument_select; // the instrument command addresses record n at this + n * INSTRUMENT_RECORD_SIZE
    uint32_t setup_list;
    uint32_t setup_extra; // JdSetupKind_FormStream: first form; JdSetupKind_PointerList: the list of form addresses
    uint32_t init;
    uint32_t play;
    int subsongs;
    uint16_t volume_mask_music;
    uint16_t volume_mask_sfx;
} JdTables;

typedef struct JdInstrument {
    int16_t base_note;
    bool untransposed; // the flag word of the record
    const int8_t* sample;
    uint16_t sample_words;
    const int8_t* repeat;
    uint16_t repeat_words;
} JdInstrument;

// The driver's channel record. Pointers are file offsets (NO_ADDR when null).
typedef struct JdChannel {
    uint16_t flags;
    uint32_t track_loop; // track list position the end command returns to
    uint32_t track_pos;  // track list position after the running pattern's number
    uint32_t pattern_loop;
    uint32_t pattern_pos;
    uint8_t pattern_repeats; // how often the running pattern still plays
    int8_t transpose;

    uint32_t envelope_start;
    uint32_t envelope_pos;
    uint8_t envelope_every;
    uint8_t envelope_countdown;
    uint16_t envelope_wait;
    int8_t envelope_step;

    uint8_t arpeggio_offset;
    uint8_t arpeggio_ticks;
    uint8_t arpeggio_countdown;

    uint8_t slide_offset;
    uint8_t slide_ticks;
    int16_t slide_step;

    uint32_t pitch_seq_start;
    uint32_t pitch_seq_pos;
    uint8_t pitch_seq_repeats;
    uint8_t pitch_seq_repeat_countdown;
    uint8_t pitch_seq_wait;
    uint8_t pitch_seq_countdown;

    uint8_t vibrato_offset;
    uint8_t vibrato_speed;
    uint8_t vibrato_delay;
    uint8_t vibrato_delay_countdown;
    uint8_t vibrato_half_period;
    uint8_t vibrato_countdown;
    int16_t vibrato_step;

    JdInstrument instrument;
    uint16_t period;
    uint16_t volume;
    uint8_t note;        // note byte with the transposition
    uint8_t played_note; // the note the period comes from now
    uint8_t fixed_length;
    uint16_t wait;    // ticks until the next pattern byte
    uint16_t dma_bit; // the channel's bit in the DMA mask

    // Display only
    int shown_note;
    int shown_instrument;

    NuadePaulaChannel paula;
} JdChannel;

struct JdPlayer {
    JdModule module;
    JdVersion version;
    JdTables tables;
    JdInstrument* instruments; // the records the setup routine filled, from tables.instruments on
    int instrument_count;

    JdChannel channels[JD_MAX_VOICES];
    uint16_t activity;    // the driver's activity word
    uint16_t dma_restart; // channels whose DMA restarts at the end of the tick
    bool looped;
    // The eagleplayer's song end: a bit per voice the subsong uses, cleared when the voice reaches the end of its
    // track list; when none is left the song has looped (or ended) and the bits come back.
    uint8_t used_voices;
    uint8_t voices_to_end;
};

// True when the data passes the eagleplayer's module check: the bra.w jump table and the setup routine's header.
bool jd_probe(const uint8_t* data, size_t size);

JdPlayer* jd_create(const uint8_t* data, size_t size);
void jd_destroy(JdPlayer* player);

int jd_subsong_count(const JdPlayer* player);
int jd_voice_count(const JdPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool jd_start(JdPlayer* player, int subsong, NuadeVoices* voices);

// Run one driver tick and hand the Paula registers it wrote to the voices.
JdTickResult jd_tick(JdPlayer* player, NuadeVoices* voices);
JdVoiceState jd_voice_state(const JdPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
JdPlayer* jd_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
