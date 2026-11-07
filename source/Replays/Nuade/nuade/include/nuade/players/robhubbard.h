// SPDX-License-Identifier: MIT
// Rob Hubbard player for the Amiga modules that carry their replayer ("*.rh", prefix rh).

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

#define RH_VOICES 4

// The CIA clock: the timebase of RH_TICK_LENGTH.
#define RH_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / RH_TIMEBASE_HZ seconds.
#define RH_TICK_LENGTH 14187

// Stream bytes below this are the length of a note; the others are voice commands.
#define RH_COMMAND_FIRST 0x80
#define RH_VARIANT_MAX_PC_TARGETS 12

typedef struct RhPlayer RhPlayer;

typedef struct RhTickResult {
    bool looped; // every voice has reached the end of its track list since the last time (the eagleplayer's SongEnd)
    bool ended;  // the replayer has stopped
} RhTickResult;

typedef struct RhVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // instrument number of the last note started, -1 if none
} RhVoiceState;

typedef enum RhCommand {
    RhCommand_None = 0,
    RhCommand_Instrument = RH_COMMAND_FIRST,
    RhCommand_Slide = 0x81,
    RhCommand_Rest = 0x82,
    RhCommand_Legato = 0x83,
    RhCommand_NextTrack = 0x84,
    RhCommand_Stop = 0x85,
    RhCommand_VolumeA = 0x86, // sets the voice's volume override (the build has one of the two)
    RhCommand_VolumeB = 0x87,
} RhCommand;

// The tables a pc-relative address of the replayer can point at.
typedef enum RhTable {
    RhTable_None, // the audio vector hook and its saved vector: not part of the music
    RhTable_Samples,
    RhTable_Instruments,
    RhTable_Pointers, // three offsets that the init stores in three instrument records
    RhTable_Specials, // the first of those records
    RhTable_Waves,
    RhTable_Periods,
    RhTable_Silence,
    RhTable_Count,
} RhTable;

// The builds of the replayer that the modules carry. They are one replayer at different stages: the fields say what a
// build does; the hash of its code recognises it and the table roles say which table each pc-relative address in the
// code is.
typedef struct RhVariant {
    const char* name;
    uint64_t code_hash;
    uint8_t pc_count;
    RhTable pc_table[RH_VARIANT_MAX_PC_TARGETS]; // the role of each pc-relative address, in code order
    // The block that a voice without a sample plays: behind the voice records (offset from the end of the code), or the
    // table of the silence role when that is 0.
    uint32_t silence_after_code;
    uint32_t variables_size; // from the end of the code to the lowest table: the voice records and, in some builds, the
                             // silence block
    uint16_t silence_words;
    uint8_t instrument_shift;  // the instrument records are 1 << shift bytes
    bool extended_instruments; // vibrato and the ferris wheel; the instrument command also picks the vibrato wave
    bool skip_negative_loop;   // a negative loop offset leaves the repeat registers alone
    bool default_instrument;   // an instrument operand with bit 7 set picks a default the init derives
    bool ignores_subsong;      // the init always starts subsong 0
    bool stop_command;         // RhCommand_Stop
    bool legato_command;       // RhCommand_Legato
    RhCommand volume_command;  // RhCommand_VolumeA or RhCommand_VolumeB, RhCommand_None: no volume override
} RhVariant;

typedef struct RhModule {
    uint8_t* data;  // the module in a nuade_paula_padded_copy; the replayer writes into it
    uint8_t* image; // the same as the eagleplayer left it, to start over from
    size_t size;
    const RhVariant* variant;
    uint32_t sample_count;
    uint32_t samples;
    uint32_t song_table;
    uint32_t subsong_count;
    uint32_t instruments;
    bool has_specials; // the init stores pointers in three instrument records
    uint32_t pointers;
    uint32_t specials;
    uint32_t waves;
    uint32_t periods;
    uint32_t silence;
    uint32_t tables_start; // the lowest table: what lies in front of it is the replayer's code and variables
} RhModule;

typedef struct RhVoice {
    uint32_t stream; // offset of the next command
    uint32_t track_list;
    uint32_t track_offset; // of the next entry of the track list
    uint16_t length;       // ticks until the next command; counts down
    bool slide;
    int8_t slide_step;
    bool legato;
    bool ferris_down;
    uint8_t note;
    uint16_t period;
    bool repeat_pending; // the note started in the tick before: the repeat registers are written now
    uint32_t instrument; // offset of the record, 0: none selected
    uint32_t wave_start;
    uint32_t wave_at;
    uint16_t ferris_at;
    uint16_t volume_override; // 0: use the instrument's volume
    bool wrapped;             // the voice has reached the end of its track list since the last SongEnd
    bool halted;              // the voice reads commands for ever in the replayer (only a module that rewrote itself)
    int shown_note;
    int shown_instrument;
    NuadePaulaChannel paula;
} RhVoice;

struct RhPlayer {
    RhModule* module;
    RhVoice voices[RH_VOICES];
    uint8_t tempo;
    uint32_t subsong; // asked for
    bool running;
    bool song_ended;
};

// True when the eagleplayer plays the module and this port reproduces it: the eagleplayer's checks, a replayer build
// that the port knows, and sequence data the port models.
bool rh_probe(const uint8_t* data, size_t size);

// null when rh_probe is false.
RhPlayer* rh_create(const uint8_t* data, size_t size);
void rh_destroy(RhPlayer* player);

int rh_subsong_count(const RhPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool rh_start(RhPlayer* player, int subsong, NuadeVoices* voices);

RhTickResult rh_tick(RhPlayer* player, NuadeVoices* voices);

RhVoiceState rh_voice_state(const RhPlayer* player, int voice);

// Ticks per unit of note length in a subsong.
int rh_subsong_tempo(const RhPlayer* player, int subsong);

// The player's state behind a song it opened; 0 when the song is another player's.
RhPlayer* rh_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
