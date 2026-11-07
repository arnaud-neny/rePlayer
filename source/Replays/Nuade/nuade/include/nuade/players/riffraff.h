// SPDX-License-Identifier: MIT
// Riff Raff player (PAL Developments games), translated from the replayers inside the modules and the UADE
// eagleplayer "RiffRaff". A song is an Amiga executable ("RIFFRAFF" header, the replayer's code, then period, rate and
// sample tables, track lists and patterns). The replayer was rewritten for each game: this port knows the four
// versions of the corpus (Protector, Side Winder II, T'Bird, Bomb Fusion) and refuses every other file.

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

#define RR_VOICES 4

// The CIA clock: the timebase of RR_TICK_LENGTH.
#define RR_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / RR_TIMEBASE_HZ seconds.
#define RR_TICK_LENGTH 14187

#define RR_TITLE_BYTES 64
#define RR_MAX_RANGES 12

typedef struct RrPlayer RrPlayer;

typedef struct RrTickResult {
    bool looped; // the replayer called SongEnd
} RrTickResult;

typedef struct RrVoiceState {
    int period;     // the period of the last note, 0 if none
    int instrument; // the instrument of the last SetSample (the voice record's start word until then)
} RrVoiceState;

typedef struct RrVoice {
    uint16_t volume;
    uint32_t sample_start; // module offset of the next note's sample
    uint16_t sample_words;
    uint16_t period;
    int16_t slide_step;
    uint16_t slide_target;
    bool slide_endless; // the slide does not stop at its target going up
    uint16_t wait;      // ticks until the next event
    int16_t volume_step;
    uint16_t volume_target;
    int16_t envelope_counter;
    uint16_t envelope_reload;
    uint32_t pattern_pos;
    uint32_t pattern_start;
    int16_t repeat;
    uint32_t track_pos;
    uint32_t track_restart;
    uint16_t transpose;
    uint16_t instrument; // the sample table entry of the last SetSample (the module's word until then); the loop tables
                         // follow it
    bool start_dma;
    NuadePaulaChannel paula;
} RrVoice;

typedef struct RrFade {
    uint16_t level;
    uint16_t target;
    uint16_t reload;
    int16_t counter;
} RrFade;

// The replayer versions differ in what a tick does; the module addresses are separate (RrLayout).
typedef enum RrDriver {
    RrDriver_Looping, // Protector, Side Winder II: instrument loop tables, the volume goes to AUDxVOL as it is
    RrDriver_Clamped, // T'Bird: as Looping, the volume is held under the master level, subsongs
    RrDriver_OneShot, // Bomb Fusion: no loop tables, one routine for both passes
} RrDriver;

typedef enum RrMaskSource {
    RrMaskSource_File,     // the word in the module
    RrMaskSource_Init,     // the init routine sets 0xF
    RrMaskSource_Subsongs, // the word behind the subsong's four track list pointers
} RrMaskSource;

// The module addresses the replayer's code names (one slot per role; the version tells where in the code each is).
typedef enum RrRole {
    RrRole_Voices, // the voice records; the four fade words lie 8 bytes in front
    RrRole_Mask,   // the voice enable word, then its working copy
    RrRole_DmaBits,
    RrRole_Periods,
    RrRole_Rates,
    RrRole_Commands, // the jump table of the pattern commands
    RrRole_SamplePtrs,
    RrRole_SampleLens,
    RrRole_SamplePtrs2, // Bomb Fusion: a second pair of tables that only feeds unused fields
    RrRole_SampleLens2,
    RrRole_LoopPtrs,
    RrRole_LoopLens,
    RrRole_Patterns,
    RrRole_Tracks, // the track list pointers of every subsong
    RrRole_StartPattern,
    RrRole_TrackStart0, // Bomb Fusion: the track list of each voice is an immediate of the init routine
    RrRole_TrackStart1,
    RrRole_TrackStart2,
    RrRole_TrackStart3,
    RrRole_Silence,
    RrRole_Extra0, // variables of the original that the port only checks (see RrVersion)
    RrRole_Extra1,
    RrRole_Extra2,
    RrRole_Count
} RrRole;

typedef struct RrSlot {
    RrRole role;
    uint16_t at; // offset of the long that holds the address, from the init entry
} RrSlot;

typedef struct RrVersion {
    const char* name;
    RrDriver driver;
    RrMaskSource mask_source;
    uint16_t record_bytes;
    uint16_t code_len;    // from the init entry to the DMA bit table
    uint16_t second_pass; // entry offsets from the init entry
    uint16_t tick;        // 0: the version has no separate tick
    uint64_t code_hash;   // the code with its relocated longs zeroed
    uint64_t role_hash;   // which role every relocated long of the code addresses
    const RrSlot* slots;
    uint32_t slot_count;
    const uint16_t* commands; // handler offsets from the init entry, the jump table entries
    uint32_t command_count;
} RrVersion;

typedef struct RrRange {
    uint32_t start;
    uint32_t end;
} RrRange;

// The module header (offsets 12 to 47 of the code hunk), as the eagleplayer's InitPlayer copies it.
typedef struct RrHeader {
    uint32_t second_pass; // entry offsets
    uint32_t init;
    uint32_t subsong_count;
    uint32_t tick; // 0: none
    uint32_t name; // offsets of the name and author strings
    uint32_t author;
    uint32_t sample_ptrs; // the sample pointer and length tables
    uint32_t sample_lens;
} RrHeader;

typedef struct RrModule {
    const RrVersion* version;
    RrHeader header;
    uint8_t* image; // nuade_paula_padded_copy of the code hunk
    uint32_t size;
    uint8_t* reloc;                 // 1..4 for the bytes of a relocated long (1 = its first byte), else 0
    uint32_t* unreadable_prefix;    // bytes up to n that are code, variables or relocated longs
    RrRange blocked[RR_MAX_RANGES]; // the code and the replayer's variables
    uint32_t blocked_count;
    uint32_t base[RrRole_Count];
    bool zero_is_base; // the eagleplayer replaces zero sample pointers by its zero bytes
    RrRange zero_fix;  // ... in these tables
    bool mask_flag;    // T'Bird: subsongs after the first load their voice mask
} RrModule;

struct RrPlayer {
    RrModule module;
    char title[RR_TITLE_BYTES];
    RrFade fade;
    bool enabled[RR_VOICES];
    bool wrap_pending[RR_VOICES]; // the eagleplayer's SongEnd flags: voices that have not restarted their track list
    bool wrap_initial[RR_VOICES];
    bool looped;
    RrVoice voices[RR_VOICES];
};

// The eagleplayer's Check2 plus the limits of what this port models: the replayer must be one of the known versions
// and every subsong must read only module data the port can reproduce.
bool rr_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `rr_probe` rejects it.
RrPlayer* rr_create(const uint8_t* data, size_t size);
void rr_destroy(RrPlayer* player);

uint32_t rr_subsong_count(const RrPlayer* player);

// The module's name (empty when it has none).
const char* rr_title(const RrPlayer* player);

// (Re)start `subsong` from its beginning; the player drives `voices` (Paula channel n = voice n).
bool rr_start(RrPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
RrTickResult rr_tick(RrPlayer* player, NuadeVoices* voices);

RrVoiceState rr_voice_state(const RrPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
RrPlayer* rr_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
