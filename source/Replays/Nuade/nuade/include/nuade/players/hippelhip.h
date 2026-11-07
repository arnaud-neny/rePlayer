// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player (the TFMX-header modules of Hippel's games: Wings of Death, Dragonflight, Warp, ...),
// translated from the replayers inside the modules and the Wanted Team eagleplayer "Jochen Hippel" that finds them.
//
// Every module starts with the replayer Hippel wrote for that game (68k code), followed by his "TFMX" header and the
// song data. The replayers come in three engines (A, B, C: successive revisions of one design) and, inside an engine,
// a few variants. The player recognises the replayer by the bytes of the module in front of the header and runs the
// matching engine; a module with a replayer it has not seen is not accepted.

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

#define HH_VOICES 4

// The CIA clock: the timebase of HH_TICK_LENGTH.
#define HH_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / HH_TIMEBASE_HZ seconds.
#define HH_TICK_LENGTH 14187

#define HH_MAX_HASHES 6

typedef struct HhPlayer HhPlayer;

typedef struct HhTickResult {
    bool looped; // the replayer called the eagleplayer's SongEnd: a track wrapped, or the song stopped and restarted
} HhTickResult;

typedef struct HhVoiceState {
    int note;       // note of the last row that started a note, -1 if none
    int instrument; // instrument (volume sequence) of that note, -1 if none
} HhVoiceState;

struct HhVersion;

typedef struct HhModule {
    uint8_t* data; // padded copy, writable: engine A's sample pack command and the subsong start patch it
    uint32_t size;
    uint32_t header; // offset of "TFMX"
    // The tables behind the header, as module offsets
    uint32_t fseqs;    // frequency sequences
    uint32_t vseqs;    // volume sequences (instruments)
    uint32_t patterns; // pattern_size bytes each
    uint32_t tracks;
    uint32_t subsongs;
    uint32_t samples;
    uint32_t sample_data;
    uint16_t pattern_size;
    uint16_t subsong_count; // the eagleplayer's subsong range: 1 .. this
} HhModule;

// AUDxLC/AUDxLEN as the replayer writes them
typedef struct HhRegisters {
    const int8_t* lc;
    uint16_t len_words;
} HhRegisters;

// Engine A: a window of a sample that moves by `delta` words every `speed` ticks
typedef struct HhSweep {
    bool active;
    bool stopped;
    bool started;
    uint32_t start; // offsets into the sample data
    uint32_t end;
    uint16_t pos; // words
    int16_t delta;
    uint16_t length;
    int8_t counter;
    uint8_t speed;
} HhSweep;

// Engine C: the repeat registers are written a tick after the sample start, or in the same tick
typedef struct HhDelayedRepeat {
    uint32_t sample;   // module offset of the sample start
    uint16_t offset;   // bytes from `sample` to the repeat
    uint16_t length;   // words
    uint8_t countdown; // the repeat is written when this runs down to 0 (1 = this tick); 0 = nothing pending
} HhDelayedRepeat;

// The voice records of the three engines in one struct; each engine uses the fields its replayer has.
typedef struct HhVoice {
    // Sequencer (all engines)
    uint32_t track;       // module offset of this voice's entry in the first position of the subsong
    uint32_t track_end;   // ... in the position after the last one
    uint32_t track_pos;   // bytes into the track of the next position
    uint32_t pattern;     // module offset of the current pattern
    uint16_t pattern_pos; // bytes into the pattern
    uint8_t note;
    bool portamento;     // the info byte of the row asks for portamento
    uint8_t porta_param; // the info byte of the previous row: portamento speed, or the alternative frequency sequence
    uint8_t note_transpose;
    uint8_t instrument_transpose;
    uint32_t porta; // portamento accumulator
    int last_instrument;

    // Frequency sequence
    uint32_t fseq;
    int32_t fseq_pos;
    uint8_t fseq_wait;
    uint8_t fseq_value; // note offset, or an absolute note with bit 7 set

    // Volume sequence
    uint32_t vseq; // module offset of the sequence behind the 5 byte header
    int32_t vseq_pos;
    uint8_t vseq_counter;
    uint8_t vseq_speed;
    uint8_t vseq_wait;
    uint8_t volume;

    // Vibrato
    uint8_t vibrato_speed;
    uint8_t vibrato_depth;
    uint16_t vibrato_value;
    uint8_t vibrato_delay;
    bool vibrato_rising;
    bool vibrato_hold; // engine A: a speed with bit 7 toggles this every tick, and a tick with it set does not move

    uint8_t current_sample; // 0xff = none / start again (engines A, B)

    // The repeat: where AUDxLC/AUDxLEN point once the sample has been started
    HhRegisters repeat;      // engines A, B
    HhDelayedRepeat delayed; // engine C
    HhSweep sweep;           // engine A

    bool dma_restart; // the replayer switched the channel's DMA off in this tick, to start it again
    uint16_t period;
    NuadePaulaChannel paula;
} HhVoice;

// A subsong table entry: first and last track position, and the speed
typedef struct HhSubsong {
    uint16_t first;
    uint16_t last;
    uint16_t speed;
} HhSubsong;

typedef struct HhSong {
    HhVoice voices[HH_VOICES];
    HhSubsong subsong;
    uint16_t counter; // ticks until the next sequencer step
    uint16_t reload;  // the speed
    bool stopped;     // a track position says stop: the next tick silences everything
    bool looped;      // SongEnd was called in this tick
    int sndnum;       // subsong number, from 1
} HhSong;

typedef struct HhEngine {
    const char* name;
    // Where the replayer keeps the note of a voice record in the module (the module file holds a dump of a running
    // replayer, which the song start does not overwrite); record_size 0: the song start clears the note.
    uint8_t record_size;
    uint8_t record_note_offset;
    bool first_step_in_first_tick; // the tick counter starts at 1, not at the speed
    bool new_note_stops_channel;   // a new note switches the channel off; a sample command starts it again
    bool alt_fseq_allowed;         // the info bit 6 of a row names another frequency sequence
    bool flat_vibrato;             // the vibrato adds the same amount at every pitch
    bool stop_keeps_periods;       // the restart after a stop leaves the periods written
    uint8_t jump_mask;             // E0 jumps to the argument & jump_mask (frequency and volume sequences)
    bool vseq_commands_hold;       // volume sequence commands other than E0 and E8 hold; otherwise they are volumes
    // The few voice fields the song start sets differently
    void (*init_voice)(HhVoice* v);
    // The frequency sequence of a voice for this tick (engine A: and the sample sweep)
    void (*run_frequency)(const HhModule* m, const struct HhVersion* version, HhVoice* v);
    // The repeat registers, after the channels the commands stopped have been started again
    void (*write_repeat)(HhSong* song, const HhModule* m, const struct HhVersion* version);
} HhEngine;

typedef struct HhVersion {
    uint32_t code_hashes[HH_MAX_HASHES]; // FNV-1a of the module bytes in front of the "TFMX" header, 0 ends the list
    const HhEngine* engine;
    uint16_t start_sequence; // module offset of the sequence every voice starts with (value 1, zeros, hold)
    uint16_t period_table;   // module offset of the replayer's period table
    uint16_t voice_records;  // module offset of its first voice record
    uint16_t request_word;   // module offset of the sound effect request word the play routine tests; 0 = none
    uint8_t voices;          // music voices (the sound effect replayers run a fourth voice only for effects)
    // Replayer variables that a note past the period table can read, as bytes in front of the header (0 = none): the
    // DMA mask of the tick (a word), the flag that the tables were set up (a word), the address of the subsong table
    // (a long), and the word that says a sound effect plays (the replayer clears it; the probe wants the file's word to
    // be zero, which is the value the model reads). Whether the play routine tests it differs: in most sound effect
    // replayers it gates the fourth voice, in Shaolin ingame 1 the fourth voice runs every tick.
    uint8_t shadow_before_header;
    uint8_t init_flag_before_header;
    uint8_t subsongs_before_header;
    uint8_t effect_before_header;
    // Engine A: while a frequency sequence waits, the sample sweeps are not run
    bool a_wait_skips_sweep;
    // Engine C
    bool c_has_switch_command; // E4 (switch the sample at the next repeat)
    uint8_t c_start_delay;     // ticks from an E2 sample start until the repeat registers are written
} HhVersion;

struct HhPlayer {
    uint8_t* original; // the file as read
    HhModule module;
    const HhVersion* version;
    HhSong song;
};

// The eagleplayer's module check (replayer code that starts with the entry branches, a "TFMX" header, a sample table
// that starts with 0 and a second sample behind the first), and a replayer this player knows.
bool hh_probe(const uint8_t* data, size_t size);

// Copies the module; 0 when it is not one this player accepts.
HhPlayer* hh_create(const uint8_t* data, size_t size);
void hh_destroy(HhPlayer* player);

int hh_subsong_count(const HhPlayer* player);
// The engine of the replayer found ("engine B"), for the metadata.
const char* hh_replayer_name(const HhPlayer* player);

// (Re)start a subsong (0-based; the replayer numbers them from 1). The player drives `voices` (Paula channel n = voice
// n).
bool hh_start(HhPlayer* player, int subsong, NuadeVoices* voices);

HhTickResult hh_tick(HhPlayer* player, NuadeVoices* voices);

// Ticks per sequencer step of the running subsong.
int hh_speed(const HhPlayer* player);

HhVoiceState hh_voice_state(const HhPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
HhPlayer* hh_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
