// SPDX-License-Identifier: MIT
// EMS v6 player (Electronic Music System v6, Sean Connolly), translated from the UADE eagleplayer "EMS-6" (mixer code
// by Jarno Paananen, adapted by Wanted Team and shd). A module is a 32 track sequencer with sampled, wavetable and
// 8SVX instruments; the player mixes the 32 tracks in software (UADE's configuration: STEREO14, 24 kHz) into four
// looping Paula buffers: the high and the low six bits of the left and the right 14-bit output.

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

#define EMS6_VOICES 4  // Paula channels: high left, high right, low right, low left
#define EMS6_TRACKS 32 // sequencer tracks, mixed into the Paula channels: the voices of the display

// The eagleplayer sets its own CIA timer; the interrupt comes every 14209 cycles of the 709379 Hz CIA clock.
#define EMS6_TIMEBASE_HZ 709379
#define EMS6_TICK_LENGTH 14209

#define EMS6_VERSION_LENGTH 4
#define E6_TRACKS EMS6_TRACKS // sequencer tracks = mixer voices

typedef struct Ems6Player Ems6Player;

typedef struct Ems6TickResult {
    bool looped; // every track has jumped back in its list or ended (the eagleplayer's SongEnd)
} Ems6TickResult;

typedef struct Ems6TrackState {
    int note;             // period table index (semitones from a C) of the last note played, -1 if none
    int instrument;       // the instrument of that note, -1 if none
    uint32_t note_starts; // notes played since the start
} Ems6TrackState;

// The four sections after the header. All offsets are into `data`; reads outside the file give 0.
typedef struct Ems6Module {
    uint8_t* data;
    uint32_t size;
    uint32_t arpeggios;   // table of offsets to arpeggio note lists
    uint32_t subsongs;    // subsong headers, then the track lists
    uint32_t patterns;    // table of offsets to patterns
    uint32_t instruments; // table of offsets to instruments, then their data
} Ems6Module;

// A subsong header: the channel count (it divides the volume table) and the offsets of the 32 track lists.
typedef struct Ems6Subsong {
    uint16_t channels;
    uint32_t track_lists[E6_TRACKS]; // module offsets
} Ems6Subsong;

typedef enum E6Side {
    E6Side_Left,
    E6Side_Right,
    E6_SIDES,
} E6Side;

typedef enum E6Ring {
    E6Ring_HighLeft,
    E6Ring_HighRight,
    E6Ring_LowLeft,
    E6Ring_LowRight,
    E6_RINGS,
} E6Ring;

typedef struct Ems6MixVoice {
    uint32_t pointer;  // module offset of the sample byte the position counts from
    int32_t remaining; // bytes left from `pointer` to the end of the sample (or of the loop)
    uint32_t loop_pointer;
    uint32_t loop_length;
    uint16_t period;   // 0 = silent; the track's note period
    uint16_t volume;   // 0 .. 64
    uint32_t position; // 16.16: the whole bytes from `pointer` in the high word, the fraction in the low word
    bool looping;
    bool stopped;
} Ems6MixVoice;

typedef struct Ems6SplitEntry {
    int8_t high;
    int8_t low; // the low six bits
} Ems6SplitEntry;

typedef struct Ems6Mixer {
    Ems6MixVoice voices[E6_TRACKS];
    int16_t* volume_table;       // 65 rows of 256 words: [volume][sample byte]
    Ems6SplitEntry* split_table; // [16-bit value >> 1]
    int16_t* mix[E6_SIDES];
    int8_t* ring[E6_RINGS];
    uint16_t channels;
    uint16_t master_volume;
    uint32_t write_pos;        // ring offset of the next byte to write
    uint32_t read_pos;         // the model of the DMA position, 24.8 fixed point
    uint32_t per_tick;         // bytes the DMA reads per interrupt, 24.8
    uint16_t seq_tick_samples; // output samples per sequencer tick
    uint16_t until_seq_tick;   // samples until the next sequencer tick
} Ems6Mixer;

typedef struct Ems6Track {
    uint8_t speed; // ticks per row of this track (0 = 256)
    uint8_t row_countdown;
    uint16_t pattern_pos;     // offset into the pattern
    uint16_t list_pos;        // offset into the track list
    uint8_t repeat;           // repeats left of the pattern
    uint8_t transpose;        // added to every note
    uint8_t note;             // the note as played (after transpose); the arpeggio and vibrato base
    uint8_t instrument_shift; // added to the instrument numbers of the patterns
    uint8_t instrument;
    uint32_t track_list; // module offset
    uint8_t note_work;   // glide start note
    uint8_t glide_target;
    uint8_t glide_steps;
    uint8_t glide_divisor;
    uint8_t glide_delay;
    bool arpeggio_on;
    uint8_t arpeggio_table;
    uint8_t arpeggio_pos;
    int8_t vibrato_phase;
    int8_t vibrato_depth;
    uint8_t vibrato_divisor;
    uint8_t vibrato_delay;
    int8_t vibrato_depth_set; // the values a note start copies
    uint8_t vibrato_divisor_set;
    uint8_t vibrato_delay_set;
    uint8_t volume; // envelope level
    uint8_t volume_target;
    uint8_t envelope_reload;
    uint8_t envelope_timer;
    uint8_t envelope_step;
    uint16_t note_length; // ticks of the note left before the release
    uint8_t release_rate;
    uint8_t cut_ticks;
    uint8_t skip; // rows the same note byte repeats
    uint8_t retrigger_timer;
    uint8_t retrigger_reload;
    uint8_t retrigger_count;
    bool looped;    // the track list jumped back
    bool ended;     // the track list ended
    bool wavetable; // the instrument is a "SYN" wavetable
    bool wave_down;
    uint8_t wave_size;
    uint8_t wave_pos;
    uint8_t wave_step;
    uint8_t wave_min;
    uint8_t wave_max;
    uint8_t wave_start;
    bool retrigger_sample; // a note (re)starts the instrument's sample
    int finetune;          // period table 0 .. 15
    uint32_t note_starts;  // notes played since the start (display only; the replayer keeps no count)
} Ems6Track;

typedef struct Ems6Sequencer {
    Ems6Track tracks[E6_TRACKS];
    uint8_t master_volume; // faded towards the target
    uint8_t master_target;
    uint8_t master_speed;
    uint8_t master_timer;
    uint8_t speed; // global ticks per row
    uint8_t speed_countdown;
} Ems6Sequencer;

struct Ems6Player {
    Ems6Module module;
    Ems6Subsong subsong;
    uint32_t used_tracks; // e6_sequencer_used_tracks of the subsong
    Ems6Mixer mixer;
    Ems6Sequencer seq;
    NuadePaulaChannel paula[EMS6_VOICES];
    uint32_t seen_wraps; // laps of channel 0 the audio interrupt has been answered for
    bool first_tick;     // the timer has not fired yet
    bool finished;       // the song end was reported
    char version[EMS6_VERSION_LENGTH + 1];
};

// The eagleplayer's Check2: the 10 bytes "E.M.S. V6.", plus the section sizes of the header fitting the file.
bool ems6_probe(const uint8_t* data, size_t size);

// 0 when the module is not accepted by ems6_probe or memory runs out. The player keeps its own copy of the data.
Ems6Player* ems6_create(const uint8_t* data, size_t size);
void ems6_destroy(Ems6Player* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ems6_start(Ems6Player* player, NuadeVoices* voices);

Ems6TickResult ems6_tick(Ems6Player* player, NuadeVoices* voices);

// The tracks the song uses (1 .. EMS6_TRACKS): the ones up to the last whose track list does not end at once.
uint32_t ems6_track_count(const Ems6Player* player);

// A track as the sequencer has it. The mixer runs ahead of the Paula DMA, so the state is up to one mix buffer early.
Ems6TrackState ems6_track_state(const Ems6Player* player, uint32_t track);

// The version of the editor that wrote the module ("6.05").
const char* ems6_version(const Ems6Player* player);

// The player's state behind a song it opened; 0 when the song is another player's.
Ems6Player* ems6_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
