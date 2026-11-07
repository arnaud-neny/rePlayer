// SPDX-License-Identifier: MIT
// Kris Hatlelid player, translated from the UADE eagleplayer "KrisHatlelid" and the sound driver it runs. A song is an
// Amiga executable of three hunks. The driver is either in the song (Test Drive 2, Grand Prix Circuit) or in a file
// "songplay" next to it, and the song file then only holds the song data. Four tracks of 8SVX (IFF) instruments play
// through the four Paula channels; the eagleplayer loads the driver as a program and patches its hardware accesses.
// Six driver versions are known.

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

#define KH_VOICES 4

// The CIA clock: the timebase of KhTickResult.length.
#define KH_TIMEBASE_HZ 709379

// The driver's data hunk is kept for its tables.
#define KH_DRIVER_DATA_BYTES 1024u

typedef struct KhPlayer KhPlayer;

typedef enum KhStatus {
    KhStatus_Ok = 0,
    KhStatus_InvalidData,
    KhStatus_MissingDriver, // the song needs the file "songplay" in its directory
    KhStatus_OutOfMemory,
} KhStatus;

typedef struct KhCreateResult {
    KhPlayer* player;
    KhStatus status;
} KhCreateResult;

typedef struct KhTickResult {
    uint32_t length; // until the next tick, in 1 / KH_TIMEBASE_HZ seconds
    bool looped;     // every track has started over since the last time: the eagleplayer calls SongEnd
    bool ended;      // the driver stopped, or all tracks are over: the eagleplayer calls SongEnd and nothing plays
} KhTickResult;

typedef struct KhMemory {
    uint8_t* bytes; // bytes[0] is at KH_LOAD_BASE; Paula can read NUADE_PAULA_MAX_SAMPLE_BYTES zeros behind `size`
    uint32_t size;
} KhMemory;

typedef enum KhTempo {
    KhTempo_HighLow = 0, // command 0xDD sets the high byte of the CIA timer, 0xDE the low byte
    KhTempo_High,        // both set the high byte; 0xDE as (~value) / 2
    KhTempo_HighScaled,  // both set the high byte; 0xDE as ((~value) * 144 / 160) / 2
} KhTempo;

typedef struct KhVariant {
    const char* name;
    uint64_t signature; // FNV-1a of the code hunk, then the first data_bytes of the data hunk (as in the file)
    uint32_t code_bytes;
    uint32_t data_bytes;
    bool external;          // the driver is the file "songplay" next to the song
    bool prepare_first;     // the song start prepares the instrument table from entry 0, not from entry 1
    bool compact_notes;     // a note byte of 0 is a rest, bit 7 of either byte ties, the instrument transposes
    bool duration_formula;  // the duration is computed from the byte, not looked up
    bool stop_song_command; // command 0xDA stops the song, not the track
    KhTempo tempo;
} KhVariant;

typedef struct KhModule {
    KhMemory song;                             // the song's hunks as loaded
    const KhVariant* variant;                  // null for a song that needs its driver file, until it is loaded
    bool external;                             // the driver is not in the song
    uint8_t driver_data[KH_DRIVER_DATA_BYTES]; // the driver's data hunk: its period and duration tables
    uint32_t driver_data_bytes;
    uint32_t song_table;    // a song with an external driver: the table of song addresses
    uint32_t embedded_song; // a song with its own driver: the song
    uint32_t subsong_count;
} KhModule;

typedef enum KhState {
    KhState_Idle = 0,
    KhState_Start, // the song start has been asked for
    KhState_Playing,
    KhState_Stopping, // a tick will switch everything off
} KhState;

// What the audio interrupt does at the next block start of a voice
typedef enum KhBlock {
    KhBlock_Once = 0,  // the sample plays once: the next interrupt stops the DMA
    KhBlock_First = 1, // the first block plays: the next interrupt writes the repeat
    KhBlock_Done = -1, // the repeat is written, or the voice stopped
} KhBlock;

typedef struct KhVoice {
    NuadePaulaChannel paula;
    bool active;              // 0x00: the track runs
    uint32_t list;            // 0x02: the track list
    uint32_t stream;          // 0x06: the next pair of bytes
    uint32_t list_pos;        // 0x0A: byte offset of the list entry the stream is from
    uint16_t duration;        // 0x0E: ticks left of the note, 0 = read the next event
    uint16_t release_at;      // 0x10: the note is released when `duration` gets here
    KhBlock block_state;      // 0x12
    uint32_t vhdr;            // 0x14: VHDR data of the instrument
    uint32_t body;            // 0x18: BODY data
    uint16_t octave_span;     // 0x1C
    uint16_t volume;          // 0x1E
    uint16_t base_period;     // 0x20: period of the note
    uint16_t repeat_words;    // 0x22: length of the repeat in words, 0 = the sample plays once
    uint32_t loop_start;      // 0x24: address of the repeat
    uint16_t hold;            // 0x38
    uint16_t tie;             // 0x3A: 2 the first tied note, 1 the following ones
    uint32_t instrument;      // 0x3C: the instrument record
    uint16_t note;            // 0x40: pitch of the note before the arpeggio
    uint16_t period;          // 0x42: current period with the vibrato
    uint16_t shown_period;    // the period of the last note started, for the display
    int16_t vibrato_step;     // 0x44
    int32_t instrument_index; // for the display
    uint32_t wraps_seen;
    bool interrupt_enabled; // INTENA of the channel's audio interrupt
    bool interrupt_pending; // INTREQ of the channel: a block started since the handler last ran
    bool started_over;      // the eagleplayer's flag for this track
} KhVoice;

struct KhPlayer {
    KhModule module;
    KhMemory memory; // the song, as the driver changes it
    uint32_t song;
    KhState state;
    bool note_seen; // a track has handled a note
    bool finished;
    uint16_t timer;
    KhTickResult tick;
    KhVoice voices[KH_VOICES];
    int8_t silence[2];
};

// The eagleplayer's Check2 (one of three layouts of the first hunk), plus what this port needs: for a song that holds
// its driver, the driver is one of the known versions and the song data loads. A song without a driver is only
// checked for its layout; `kh_create` also needs its driver file.
bool kh_probe(const uint8_t* data, size_t size);

// Loads the song and its driver (`path` and `io` find "songplay"; a path without a directory means the current one).
KhCreateResult kh_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void kh_destroy(KhPlayer* player);

uint32_t kh_subsong_count(const KhPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool kh_start(KhPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one driver tick and hand the Paula registers it wrote to the voices.
KhTickResult kh_tick(KhPlayer* player, NuadeVoices* voices);

// The period of the last note a voice started, 0 if none.
uint16_t kh_voice_period(const KhPlayer* player, uint32_t voice);

// The instrument the voice last selected, -1 if none.
int32_t kh_voice_instrument(const KhPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
KhPlayer* kh_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
