// SPDX-License-Identifier: MIT
// Dirk Bialluch player (the music of "Timelock", 1993), translated from the UADE eagleplayer "Dirk Bialluch" (Wanted
// Team). A song is two files: "tpu.<name>" (sequence, patterns, programs, groove and arpeggio tables) and "smp.<name>"
// (the 8-bit samples). The eagleplayer holds the replayer.

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

#define DB_VOICES 4

// The CIA clock: the timebase of DB_TICK_LENGTH.
#define DB_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / DB_TIMEBASE_HZ seconds.
#define DB_TICK_LENGTH 14187

typedef struct DbPlayer DbPlayer;

typedef enum DbStatus {
    DbStatus_Ok = 0,
    DbStatus_InvalidData,
    DbStatus_MissingSamples, // the "smp." file next to the song could not be read, or is not an MSMP chunk
    DbStatus_OutOfMemory,
} DbStatus;

typedef struct DbCreateResult {
    DbPlayer* player;
    DbStatus status;
} DbCreateResult;

typedef struct DbTickResult {
    bool looped; // a pattern jumped to a song position, or stopped: the eagleplayer calls SongEnd
    bool ended;  // the pattern stopped the player: later ticks do nothing
} DbTickResult;

typedef struct DbVoiceState {
    int note;       // semitones from C-0 (12 = C-1) of the last note started, -1 if none
    int instrument; // the program number of that note, -1 if none
} DbVoiceState;

typedef struct DbModule {
    const uint8_t* data; // the song file
    uint32_t size;
    uint32_t song_table;    // MSNG: a start position word per subsong
    uint32_t sequence;      // MSEQ data
    uint32_t pattern_table; // MPTN: word offsets
    uint32_t pattern_data;
    uint32_t program_table; // MPRG: word offsets
    uint32_t program_data;
    uint32_t groove;   // MGRV data
    uint32_t arpeggio; // MARP data
    uint32_t subsong_count;
    const uint8_t* samples; // the sample file and NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes (nuade_paula_padded_copy)
    uint32_t samples_size;  // the sample file's size
    uint32_t sample_table;  // MSMP: long offsets
    uint32_t sample_data;
} DbModule;

typedef struct DbVoice {
    NuadePaulaChannel paula;
    // The pattern stream
    uint32_t pattern;         // address of the pattern in the song file
    uint8_t pattern_position; // byte offset in the pattern
    uint8_t wait;             // ticks of rows to wait
    uint8_t loop_count;
    uint8_t transpose;
    uint8_t note_offset;       // (note + transpose) * 2: byte offset into the period table
    uint8_t requested_program; // the program number a note asked for; 0 for none
    uint8_t signal;            // set by a pattern command, tested by a program
    // The program
    uint32_t program;         // address of the running program in the song file
    uint8_t program_position; // byte offset after the program header
    uint8_t program_wait;
    uint8_t program_loop_count;
    uint8_t volume;
    uint8_t volume_offset;
    uint8_t fixed_note; // period table entry (not offset); 0 for none
    uint8_t arpeggio_start;
    uint8_t arpeggio_position;
    uint8_t slide_target_offset; // period table byte offset
    uint8_t slide_rate;
    int16_t slide_offset;    // added to the period while a slide runs
    uint16_t bend;           // added to the period
    uint32_t sample_address; // AUDxLC as an address of the model (see DB_SAMPLE_FILE_ADDRESS)
    uint16_t length_words;   // AUDxLEN
    // Display
    int note;
    int instrument;
} DbVoice;

struct DbPlayer {
    DbModule module;
    DbVoice voices[DB_VOICES];
    bool running;
    bool step_pending;   // the next sequencer turn loads the step at `position` first
    uint16_t position;   // song position: the step to load next
    uint8_t row_counter; // sequencer turns to skip, set by the groove
    uint8_t groove_offset;
    uint8_t groove_position;
    uint8_t master_volume;
    uint8_t fade_step;
    uint8_t fade_interval;
    uint8_t fade_counter;
    uint8_t fade_subsong; // 1-based subsong the fade switches to; 0 for no fade
};

// The eagleplayer's Check2 ("MSNG", then "MSEQ" within the first 0x204 bytes, only words below 0x1000 before it) and
// the chunk search that InitPlayer makes: MSEQ, MPTN, MPRG, MGRV and MARP in this order.
bool db_probe(const uint8_t* data, size_t size);

// Parses the song (InitPlayer) and loads the sample file: the song's file name with its "tpu." prefix changed to
// "smp.", in the song's directory.
DbCreateResult db_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void db_destroy(DbPlayer* player);

int db_subsong_count(const DbPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool db_start(DbPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
DbTickResult db_tick(DbPlayer* player, NuadeVoices* voices);

// Ticks one row of the display grid spans: the first groove step + 1.
uint32_t db_grid_ticks(const DbPlayer* player);

DbVoiceState db_voice_state(const DbPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
DbPlayer* db_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
