// SPDX-License-Identifier: MIT
// Dave Lowe player (Uncle Art / Dave Lowe sound system, 1988-94), translated from the UADE eagleplayers "Dave Lowe",
// "Dave Lowe Deli" and "Dave Lowe New" (Wanted Team, Darkness/Synergy). One sound engine, three kinds of module:
//   - the game's own driver: an Amiga hunk executable ("UNCLEART" tag) that carries the engine, tables and music;
//   - the same engine ripped into the Deli player, music modules with a "DAVELOWE" tag;
//   - the engine in the eagleplayer, data-only ".dln" modules (offsets relative to the module start).
// The three differ in how the song data is found and in small details of the engine; the voice logic is shared.

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

#define DL_VOICES 4

// The CIA clock: the timebase of DL_TICK_LENGTH.
#define DL_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / DL_TIMEBASE_HZ seconds.
#define DL_TICK_LENGTH 14187

#define DL_MAX_COMMANDS 17
#define DL_EFFECT_CHANNELS 2

typedef struct DlPlayer DlPlayer;

typedef struct DlTickResult {
    bool looped; // every voice reached the end of its list since the last time they all had
} DlTickResult;

typedef struct DlVoiceState {
    int note;       // semitones from C-0 of the voice's last note, -1 if none yet
    int instrument; // index of the sample record the voice plays, -1 if unknown
} DlVoiceState;

typedef enum DlKind {
    DlKind_Game,
    DlKind_Deli,
    DlKind_New,
} DlKind;

// The tables the engine points voices at before the song data has: a list of no patterns, a sequence that asks for the
// next pattern, and a volume envelope that holds zero. The game drivers carry their own copies; these have the same
// contents.
typedef struct DlBuiltin {
    uint32_t empty_list;
    uint32_t idle_sequence;
    uint32_t silent_envelope;
} DlBuiltin;

typedef struct DlImage {
    uint8_t* data;
    uint8_t* original; // the module after the load fixes, to restore the data a song changes while it plays
    uint32_t module_size;
    uint32_t size; // the module bytes, the padding and the built-in tables
    DlBuiltin builtin;
    uint32_t builtin_free; // where more built-in data can go
} DlImage;

// The state of a voice's sample. The values are those of the state word of a sample record, except Started, which a
// record can have too but the engine sets when a note starts.
typedef enum DlSampleState {
    DlSampleState_Immediate = 0, // the sample record put the sound into the registers
    DlSampleState_Looping = 1,   // a note restarts the channel: first the sound, one tick later its loop part
    DlSampleState_Started = 3,   // a note has just started; the next tick writes the loop part
} DlSampleState;

// What a subsong starts from.
typedef struct DlSubsong {
    uint32_t lists[DL_VOICES]; // per voice: the list of pattern addresses, ended by a 0 entry
    uint32_t tick_limit;       // see DlVariant.tick_skip
    uint16_t fade_speed;       // see DlVariant.fade: volume writes between two steps of the fade-in, 0 = no fade
    uint16_t ramp_ticks;       // the music starts silent and rises one volume step every ramp_ticks ticks, 0 = no ramp
    uint32_t effect_request;   // a sound effect the eagleplayer asks for at the start (see DlVariant.effect_table)
} DlSubsong;

// The commands of a pattern. A command word is four times the index of the command in the engine's command table; the
// tables differ between the engines, so each variant lists what its indices mean.
typedef enum DlCommand {
    DlCommand_Freeze = 0, // does nothing and is read again on the next tick: the voice stands still
    DlCommand_Sample,     // select the sound of the next notes
    DlCommand_NextPattern,
    DlCommand_Envelope,
    DlCommand_Slide,
    DlCommand_Vibrato,
    DlCommand_SlideOff,
    DlCommand_VibratoOff,
    DlCommand_Rest,
    DlCommand_Stop,
    DlCommand_Filter, // switches the Amiga's LED filter: no sound of its own here
    DlCommand_BonusOn,
    DlCommand_Jump,
    DlCommand_Repeat,  // jump back a counted number of times; the counter lives in the pattern (Frontier)
    DlCommand_Restart, // runs the driver's init routine again (one Galaxy Force 2 pattern)
    DlCommand_SongEnd, // the song has ended: tells the host, then starts it again (Deli modules)
} DlCommand;

// What differs between the engines of the three kinds of module and between the game drivers.
typedef struct DlVariant {
    DlCommand commands[DL_MAX_COMMANDS];
    uint32_t command_count;    // a command word outside the table makes the voice stand still
    bool tick_skip;            // the engine skips one tick in tick_limit + 1 (tempo adjustment of the new engine)
    bool fade;                 // the volumes go through a fade-in that the subsong's fade speed starts (Deli engine)
    bool dma_every_tick;       // a voice switches its DMA on at the end of every tick but the one that ends its note
    bool envelope_adds_volume; // old engines: a command makes the envelope values add the volume bonus of the header
    uint32_t envelope_header_words; // words before the envelope values: 2 (volume cut, volume bonus), 1 or 0
    bool vibrato_delay;             // the vibrato command's delay words are used (the early engine ignores them)
    uint32_t effect_table; // the table of sound effect patterns of a driver that plays effects on channels 2 and 3
} DlVariant;

// The game drivers: an executable with the engine in it
typedef struct DlGameModule {
    DlVariant variant;
    uint32_t effect_request; // the sound effect the eagleplayer asks for in the first subsong, 0 = none
    uint32_t ramp_subsong;   // the subsong (1-based) whose init starts the volume ramp, 0 = none
    uint16_t ramp_ticks;
    uint32_t subsong_table; // 0: the module has one song, in `single_song`
    DlSubsong single_song;
    uint32_t subsong_count;
    uint32_t sample_records;
} DlGameModule;

// The Deli modules: a data-only hunk executable ("DAVELOWE" tag, a subsong table of 22 byte entries at 0x10)
typedef struct DlDeliModule {
    uint32_t subsong_count;
    uint32_t table; // the subsong table
} DlDeliModule;

// The data-only ".dln" modules of the "Dave Lowe New" eagleplayer
typedef struct DlNewModule {
    bool engine_a;  // the first engine (Beneath a Steel Sky, King's Quest 6); else the second (Street Fighter 2)
    uint32_t table; // the subsong table
    uint32_t subsong_count;
    uint32_t first_sample_record;
} DlNewModule;

typedef enum DlSlideMode {
    DlSlideMode_Off,
    DlSlideMode_Up,
    DlSlideMode_Down,
} DlSlideMode;

typedef struct DlSlide {
    DlSlideMode mode;
    uint16_t step;
    uint16_t target;
    uint16_t delay;
} DlSlide;

// The vibrato moves the period down twice, then up twice
typedef enum DlVibratoPhase {
    DlVibratoPhase_FirstDown,
    DlVibratoPhase_SecondDown,
    DlVibratoPhase_FirstUp,
    DlVibratoPhase_SecondUp,
} DlVibratoPhase;

typedef struct DlVibrato {
    bool active;
    DlVibratoPhase phase;
    uint16_t counter;
    uint16_t reload;
    uint16_t up;
    uint16_t down;
    uint16_t delay;
    uint16_t delay_reload;
} DlVibrato;

// Where the register and DMA writes of a voice go. A voice of the music writes the registers of its own channel, unless
// a sound effect has the channel: then the voice writes a set of registers that no channel reads, but its DMA requests
// still reach the channel (and its DMA stop requests do on the tick the effect starts).
typedef struct DlBus {
    NuadePaulaChannel* registers;
    NuadePaulaChannel* dma_on;
    NuadePaulaChannel* dma_off; // null: the request is lost
} DlBus;

typedef struct DlVoice {
    uint32_t channel; // the Paula channel of the voice's own music; also the index in the loop tracker
    bool effect;      // a voice of a sound effect, not of the music
    uint16_t count;
    DlSampleState sample_state;
    uint16_t period;
    DlSlide slide;
    DlVibrato vibrato;
    uint32_t envelope; // read position
    uint32_t envelope_start;
    uint16_t volume_cut;
    uint16_t volume_bonus;
    bool bonus_active;
    uint32_t sequence; // read position in the pattern
    uint32_t list;     // read position in the pattern list
    uint32_t list_start;
    uint32_t sample_address;
    uint16_t sample_length_words;
    uint32_t loop_address;
    uint16_t loop_length_words;
    uint32_t sample_record; // address of the record the voice plays, for the display
    int note;
    DlBus bus; // set before every update
} DlVoice;

// The song loop: the engine reports the end of the song when every voice has reached the end of its list. The
// start-up sequence counts as one end, so each voice has to report twice the first time.
typedef struct DlLoopTracker {
    int ends_needed[DL_VOICES];
    bool looped;
} DlLoopTracker;

// The Deli engine's fade-in: the volume scale rises by `step` every `reload` + 1 volume writes until it is 0x40
typedef struct DlFade {
    uint8_t level;
    uint8_t step;
    uint16_t reload;
    uint16_t counter;
} DlFade;

typedef struct DlRamp {
    uint16_t level;
    uint16_t ticks;
    uint16_t counter;
} DlRamp;

// A sound effect on channel 2 or 3: while it plays, the music voice of the channel writes nowhere and every music voice
// gets quieter by the volume cut of its envelope.
typedef struct DlEffectChannel {
    bool active;
    uint32_t pending; // the effect to start on the next tick, 0 = none
    DlVoice voice;
} DlEffectChannel;

typedef struct DlEngine {
    DlImage* image;
    DlVariant variant;
    DlVoice voices[DL_VOICES];
    NuadePaulaChannel channels[DL_VOICES];
    NuadePaulaChannel unconnected; // the registers of a music voice whose channel plays an effect
    DlLoopTracker loop;
    DlSubsong subsong;
    DlFade fade;
    DlRamp ramp;
    uint32_t effect_request;
    DlEffectChannel effects[DL_EFFECT_CHANNELS];
    uint32_t tick_counter;
} DlEngine;

struct DlPlayer {
    DlImage* image;
    DlKind kind;
    DlGameModule game;
    DlDeliModule deli;
    DlNewModule new_module;
    DlVariant variant;
    uint32_t subsong_count;
    uint32_t first_sample_record; // for the display: sample records are 14 bytes each
    DlEngine engine;
};

// The eagleplayers' Check2 of the three kinds of module.
bool dl_probe(const uint8_t* data, size_t size);

// Copies the module and locates its tables; 0 when the data is not a known Dave Lowe module.
DlPlayer* dl_create(const uint8_t* data, size_t size);
void dl_destroy(DlPlayer* player);

uint32_t dl_subsong_count(const DlPlayer* player);
// "game driver", "Deli module" or "new module" (with the engine version), for the metadata.
const char* dl_kind_name(const DlPlayer* player);

// (Re)start a subsong (0-based) from its beginning.
bool dl_start(DlPlayer* player, uint32_t subsong);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
DlTickResult dl_tick(DlPlayer* player, NuadeVoices* voices);

DlVoiceState dl_voice_state(const DlPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
DlPlayer* dl_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
