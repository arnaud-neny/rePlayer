// SPDX-License-Identifier: MIT
// Ben Daglish game sound drivers ("DAGLISH!" modules, `bds`), translated from the UADE eagleplayer "BenDaglish-SID"
// (Wanted Team). The module format and the replayer are described in players/bendaglishsid/.

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

#define BDS_VOICES 3

// The CIA clock: the timebase of BDS_TICK_LENGTH.
#define BDS_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / BDS_TIMEBASE_HZ seconds.
#define BDS_TICK_LENGTH 14187

#define BDS_WAVE_TABLES 3 // wave pointer tables, selected by flag bits 4 to 6 of an instrument

typedef struct BdsPlayer BdsPlayer;

typedef enum BdsStatus {
    BdsStatus_Ok = 0,
    BdsStatus_InvalidData,
    BdsStatus_OutOfMemory,
} BdsStatus;

typedef struct BdsCreateResult {
    BdsPlayer* player;
    BdsStatus status;
} BdsCreateResult;

typedef struct BdsTickResult {
    bool looped; // the driver restarted the song, where the eagleplayer calls SongEnd
} BdsTickResult;

// The song's title, author and notice (empty when the module has none).
typedef struct BdsInfo {
    char title[64];
    char author[64];
    char notice[256];
} BdsInfo;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct BdsVoiceState {
    int note;
    int instrument;
} BdsVoiceState;

typedef struct BdsMemory {
    uint8_t* data;
    uint32_t size;
} BdsMemory;

typedef struct BdsHeader {
    uint32_t play;      // the driver's tick routine
    uint32_t audio_irq; // the audio interrupt handler
    uint32_t init;      // start a song
    uint32_t subsongs;  // number of subsongs
    uint32_t title;     // strings
    uint32_t author;
    uint32_t notice;
} BdsHeader;

typedef enum BdsRevision {
    BdsRevision_Early = 0,
    BdsRevision_Late,
} BdsRevision;

typedef struct BdsTables {
    // Early: the order list of each voice. Late: the variable that holds the offset of the voice's order list.
    uint32_t order_lists[BDS_VOICES];
    uint32_t subsong_orders;               // late: three order list offsets per subsong
    uint32_t tracks;                       // offset of each track, by track number
    uint32_t default_track;                // a track that ends at once, every voice starts on it
    uint32_t wave_tables[BDS_WAVE_TABLES]; // per octave: the offset of a wave form
    uint32_t wave_lengths;                 // per octave: the length of the wave forms in words
    uint32_t periods;                      // one octave of periods
    uint32_t instrument_map;               // instrument number of a voice to offset in the instrument table
    uint32_t instrument_map_default;       // the map at the start of a song
    uint32_t instruments;                  // BDS_INSTRUMENT_SIZE bytes each
    uint32_t attack_rates;                 // envelope rates, by nibble
    uint32_t decay_rates;
    uint32_t vibratos;
    uint32_t arpeggios;
    uint32_t sample_table;    // late: offset of each sample instrument's record
    uint32_t idle_wave_table; // the wave pointer table of a voice before its first instrument
    uint32_t silence_lc;      // early: what a voice plays while its instrument is muted
    // The audio interrupt's volume variables. Early: a byte and a word. Late: bytes, and the cap.
    uint32_t volume_unscaled;
    uint32_t volume_enable;
    uint32_t volume_cap;
} BdsTables;

typedef struct BdsModule {
    BdsMemory memory;
    uint8_t* pristine; // the hunk as loaded: the driver patches its own data (the instrument map)
    BdsHeader header;
    BdsRevision revision;
    BdsTables tables;
} BdsModule;

typedef enum BdsSampleState {
    BdsSampleState_None = 0, // a wave form voice
    BdsSampleState_Done,     // a sample instrument, played once
    BdsSampleState_Playing,  // a sample instrument, started and not yet through its first pass
} BdsSampleState;

// The driver's record of a voice. The start of a song clears all of it.
typedef struct BdsVoice {
    // What the tick hands to the hardware
    uint32_t written_lc;     // the AUDxLC last written
    uint32_t lc;             // the wave form or sample of the note
    uint32_t wave_table;     // the wave pointer table of the instrument
    uint16_t len_words;      // AUDxLEN of the note
    uint16_t period;         // 0 stops the voice
    uint16_t written_volume; // late: the AUDxVOL last written by the tick
    BdsSampleState sample_state;

    // Sequencer
    uint32_t order_list;
    uint8_t order_pos;
    uint32_t track;
    uint8_t track_pos;
    uint8_t transpose;
    uint8_t repeat;     // plays of the current track that are left
    uint8_t detune;     // added to the period
    uint8_t note_ticks; // ticks to the next event; 1 starts the next one on the following tick
    uint8_t rest_ticks; // a rest: ticks left
    uint8_t note;       // the note, with transposition

    // Envelope
    uint8_t volume;
    uint8_t volume_max;
    uint8_t gate_ticks;
    bool gate;
    bool attack_done;
    uint8_t attack_step;
    uint8_t attack_interval;
    uint8_t attack_counter;
    bool decay_done;
    uint8_t sustain_level;
    uint8_t decay_step;
    uint8_t decay_interval;
    uint8_t decay_counter;
    bool release_done;
    uint8_t release_step;
    uint8_t release_interval;
    uint8_t release_counter;

    // Vibrato
    uint8_t vibrato_table;
    uint8_t vibrato_delay;
    uint8_t vibrato_phase; // 1 to 8 within a half wave
    bool vibrato_down;     // the second half wave

    // Arpeggio
    uint8_t arpeggio;      // offset in the arpeggio table, 0 = none
    uint8_t arpeggio_step; // 0xFF: finished
    uint8_t arpeggio_ticks;

    // Portamento
    uint8_t portamento_ticks; // 0xFF runs for ever
    uint8_t portamento_delay;
    uint8_t portamento_delay_reload;
    bool portamento_up;
    uint16_t portamento_step;

} BdsVoice;

// What the voice does to its Paula channel; the song start does not clear it.
typedef struct BdsChannel {
    NuadePaulaChannel paula;
    bool irq_enabled;        // the audio interrupt of the channel is enabled
    bool irq_request;        // and a block has started since it last ran
    uint32_t irq_wraps;      // the wraps of the voice that the request has seen
    bool sample_pass_active; // a sample plays its first pass as a one-shot
    bool plays_wave;         // early: the voice's instrument is not muted
    int display_note;
    int display_instrument;
} BdsChannel;

struct BdsPlayer {
    BdsModule module;
    BdsVoice voices[BDS_VOICES];
    BdsChannel channels[BDS_VOICES];
    NuadeVoices* mixer;
    double sample_rate; // of the mixer: frames of a tick's block starts
    int subsong;
    bool note_ended;  // a voice's last tick of a note: this tick writes nothing to Paula
    bool song_active; // late: cleared when an order list ends
    bool looped;
};

// The eagleplayer's check: an AmigaDOS executable with the `DAGLISH!` header, and a driver this player knows.
bool bds_probe(const uint8_t* data, size_t size);

// Loads the executable (the eagleplayer's InitPlayer) and locates the driver's tables in its code.
BdsCreateResult bds_create(const uint8_t* data, size_t size);
void bds_destroy(BdsPlayer* player);

int bds_subsong_count(const BdsPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n), a mixer
// that renders at `sample_rate`.
bool bds_start(BdsPlayer* player, int subsong, uint32_t sample_rate, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
BdsTickResult bds_tick(BdsPlayer* player, NuadeVoices* voices);
BdsInfo bds_info(const BdsPlayer* player);
BdsVoiceState bds_voice_state(const BdsPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
BdsPlayer* bds_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
