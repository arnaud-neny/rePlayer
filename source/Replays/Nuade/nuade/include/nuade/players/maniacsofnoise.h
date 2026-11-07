// SPDX-License-Identifier: MIT
// Maniacs of Noise player (the music driver of Charles Deenen's and Jeroen Tel's game music), translated from the
// UADE eagleplayer "ManiacsOfNoise". The replayer is in the module: the eagleplayer checks the module's three entry
// jumps and calls them. The player recognises the five versions of the replayer that occur in the modules by a
// signature of their code and runs a translation of that version on the module's data.

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

#define MON_VOICES 4

// The CIA clock: the timebase of MON_TICK_LENGTH.
#define MON_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MON_TIMEBASE_HZ seconds.
#define MON_TICK_LENGTH 14187

typedef struct MonPlayer MonPlayer;

typedef struct MonTickResult {
    bool ended; // the fade-out has run to silence: the replayer switched all channels off and plays nothing more
} MonTickResult;

typedef struct MonVoiceState {
    int note;       // note of the voice's last note-on (semitones from C-0), -1 if none yet
    int instrument; // instrument number of the voice, -1 if none yet
} MonVoiceState;

typedef enum MonVersion {
    MonVersion_Gyroscope = 0,
    MonVersion_FlimbosQuest,
    MonVersion_DeathTrap,
    MonVersion_DragonWars,
    MonVersion_MysteryWorldsEnd,
} MonVersion;

// What differs between the versions and is not in the code the signature covers. Offsets are from the data block
// unless said otherwise.
typedef struct MonLayout {
    MonVersion version;
    uint64_t signature;       // FNV-1a over the replayer's code, see mon_module.c
    uint32_t code_start;      // pinned offset of the code when the code addresses the image absolutely, else 0
    uint32_t interrupt_entry; // from the first entry: the interrupt routine
    uint32_t start_entry;     // from the first entry: the start-song routine
    uint32_t base_entry;      // from the first entry: the data block
    uint32_t song_table;      // the song data (song records first)
    uint32_t song_stride;     // bytes of a song record
    uint32_t period_table;    //
    uint32_t silent_buffer;   // the 8 bytes a channel without loop plays
    uint32_t subsong_count;   // byte
    uint32_t envelope_mask;   // byte, the voices whose envelope runs when the song starts
    uint32_t wave_table;      // MonVersion_MysteryWorldsEnd: absolute address of the wave table
    uint32_t voice_records;   // the first voice's record; the init routine leaves several fields as the image has them
    uint32_t voice_stride;    //
    uint32_t record_volume_high;  // the byte before the volume
    uint32_t record_period_fixed; // 0: the version has no fixed period
    uint32_t record_swing;        // 0: the version has no swing
    uint32_t record_slide_length;
    uint32_t record_slide_from;
} MonLayout;

typedef struct MonModule {
    const uint8_t* bytes; // the image; a sample may run past its end, see mon_create
    uint32_t size;
    const MonLayout* layout;
    uint32_t base;  // the data block
    uint32_t songs; // the song data
    uint32_t subsong_count;
    uint32_t instruments; // the instrument records, behind the song records
} MonModule;

// Where a note is in its sample start: a note-on stops the DMA, the next tick writes the sample and starts it, the one
// after writes the loop.
typedef enum MonStart {
    MonStart_Idle = 0,
    MonStart_Loop,   // the DMA runs: write the loop
    MonStart_Fresh,  // a note-on has stopped the DMA: write the sample and start it
    MonStart_Legato, // a slide note: no restart
} MonStart;

typedef struct MonVoice {
    bool running;           // the song gives the voice a sequence
    bool envelope_on;       // the voice's envelope and slides run
    uint32_t track;         // the voice's sequence
    uint32_t sequence;      // the next sequence word
    uint32_t pattern_start; // the pattern the sequence word selected
    uint32_t pattern;       // the next pattern byte
    uint32_t instrument;    // address of the instrument record, MON_NO_ADDRESS until the first instrument command
    uint8_t volume_high;    // the high byte of the voice volume word
    uint8_t volume;
    uint8_t repeat; // plays of the current pattern left
    uint8_t note;   // note + transpose of the last note-on
    uint8_t transpose;
    MonStart start;
    bool restart; // the sequence has to be read before the pattern
    bool envelope_hold;
    uint8_t swing; // ticks the next length loses
    uint16_t ticks_left;
    uint16_t note_period;
    uint16_t base_period;  // the slide's current period
    uint16_t period;       // AUDxPER
    uint16_t period_fixed; // non-zero: AUDxPER takes it instead
    uint16_t env_volume;   // AUDxVOL
    uint16_t slide_length;
    uint16_t slide_step;
    uint16_t slide_remainder; // MonVersion_Gyroscope
    uint16_t slide_from;
    int16_t slide_delta;
    uint16_t arpeggio_offset;
    uint16_t arpeggio_list;
    uint16_t vibrato_offset;
    uint8_t vibrato_delay;
    uint8_t vibrato_timer;
    uint8_t vibrato_pos;
    uint8_t envelope_timer;
    uint8_t envelope_pos;
    uint8_t arpeggio_timer;
    uint8_t arpeggio_pos;
    bool new_instrument;
    uint32_t wave; // MonVersion_MysteryWorldsEnd: the wave in use
    uint16_t wave_length;
    uint16_t wave_pos;
    uint8_t wave_speed;
    uint8_t wave_timer;
    int display_note;
    int display_instrument;
} MonVoice;

typedef struct MonFade {
    int8_t direction; // 1 up, -1 down, 0 none
    uint8_t reload;
    uint8_t counter;
    uint8_t target;
} MonFade;

struct MonPlayer {
    MonModule module;
    uint8_t* chip; // the image with the zeros behind it that a sample may run into
    NuadePaulaChannel paula[MON_VOICES];
    MonVoice voices[MON_VOICES];
    bool playing;
    uint32_t instrument_register; // the instrument the replayer works with: a voice that reads its pattern without
                                  // having loaded its own (the first tick) uses the previous voice's
    uint8_t master;               // the song volume, the fade changes it
    uint8_t tempo;
    uint8_t start_volume; // the song's volume record: the target of a fade-in
    MonFade fade;
};

// The eagleplayer's Check2 plus the signature of the module's replayer: false when either would fail.
bool mon_probe(const uint8_t* data, size_t size);

// Copies the module and locates its replayer; null when mon_probe fails.
MonPlayer* mon_create(const uint8_t* data, size_t size);
void mon_destroy(MonPlayer* player);

int mon_subsong_count(const MonPlayer* player);

// (Re)start a subsong (0-based) from its beginning; false when the subsong does not exist.
bool mon_start(MonPlayer* player, int subsong, NuadeVoices* voices);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
MonTickResult mon_tick(MonPlayer* player, NuadeVoices* voices);

MonVoiceState mon_voice_state(const MonPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
MonPlayer* mon_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
