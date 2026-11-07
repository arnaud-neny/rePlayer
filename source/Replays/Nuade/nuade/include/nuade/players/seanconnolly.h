// SPDX-License-Identifier: MIT
// Sean Connolly player (Creative Edge games), translated from the replayers inside the modules and the UADE
// eagleplayer "SeanConnolly" (Wanted Team). A song (".scn") is the composer's own player followed by its tables: the
// module starts with a table of `bra.w` to the replayer's routines. The replayer was rewritten between games: this port
// knows the versions of the corpus (the four-voice player with a pattern/instrument sequencer in two builds, and the
// older one with 8SVX samples) and refuses every other file.

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

#define SCO_VOICES 4

// The CIA clock: the timebase of SCO_TICK_LENGTH.
#define SCO_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SCO_TIMEBASE_HZ seconds.
#define SCO_TICK_LENGTH 14187

typedef struct ScoPlayer ScoPlayer;

typedef struct ScoTickResult {
    bool looped; // the eagleplayer called SongEnd (every voice reached its end) and started the song again
    bool ended;  // the song reached something the port does not model
} ScoTickResult;

typedef struct ScoVoiceState {
    int period;     // the period of the last note, 0 if none
    int instrument; // the instrument of the last note, -1 if none
} ScoVoiceState;

typedef enum ScoRole {
    ScoRole_Songs,         // 16 bytes per subsong: the stream offset of each voice
    ScoRole_Streams,       // the stream offsets are relative to this
    ScoRole_PatternData,   // the pattern offsets are relative to this
    ScoRole_PatternTable,  // long offset per pattern
    ScoRole_Instruments,   // 16 bytes per instrument
    ScoRole_InstWaveTable, // sequencer build: long offset per wave sequence
    ScoRole_InstWaveBase,
    ScoRole_SeqTable, // long offset per pitch sequence (arpeggio)
    ScoRole_SeqBase,
    ScoRole_Period,        // word per note
    ScoRole_Wave8,         // wave forms of 8 bytes
    ScoRole_Wave16,        // sequencer build: wave forms of 16 and 32 bytes
    ScoRole_WaveSequences, // sample build: records of 64 bytes
    ScoRole_SampleTable,   // long offset per sample (its end is the next entry)
    ScoRole_SampleBase,
    ScoRole_Sfx, // the sound effect table: only the effect routines read it, which the eagleplayer never calls
    ScoRole_Count
} ScoRole;

typedef struct ScoSlot {
    ScoRole role;
    uint16_t at; // offset of the displacement word from the init entry
} ScoSlot;

// The two families are different programs: the pattern sequencer with 16-byte instrument records and the older sample
// player.
typedef enum ScoFamily {
    ScoFamily_Sequencer,
    ScoFamily_Sampler,
} ScoFamily;

typedef struct ScoVersion {
    const char* name;
    ScoFamily family;
    uint16_t prefix_bytes; // hashed in front of the init entry (the DMA bit table of the sequencer build)
    uint16_t code_len;     // from the init entry to the end of the code, where the period table starts
    uint8_t tick_bra;      // the bra.w of the module's header that leads to the interrupt routine
    uint16_t tick_at;      // ... and where it must lead, from the init entry
    uint64_t hash;         // FNV-1a of [entry - prefix, entry + code_len) with the slot words zeroed
    const ScoSlot* slots;
    uint32_t slot_count;
    uint32_t required_roles; // bit mask of the roles the replayer reads
    int16_t var_at;          // the replayer's variables start here, from the init entry
    int16_t noise_at;        // the noise buffer in the replayer's variables, from the init entry
    int16_t seed_at;         // the generator's seed words, from the init entry
    uint16_t noise_words;    // its length as a voice plays it (the buffer is twice that many bytes)
    bool noise_alternates;   // the sequencer's second build fills half a buffer per call
    // the pitch sequence command goes on into the instrument command's second half (adds the instrument offset to the
    // next-but-one byte, stores it as the pitch sequence)
    bool pitch_command_repeats;
} ScoVersion;

typedef struct ScoModule {
    const ScoVersion* version;
    uint8_t* original; // the file
    uint8_t* image;    // nuade_paula_padded_copy of the file, the replayer's working memory (variables and noise)
    uint32_t size;
    uint32_t entry; // the init routine
    uint32_t base[ScoRole_Count];
    uint32_t subsong_count;
    uint32_t var_start; // the replayer's variables [var_start, entry) are live state in the original, not module data
    bool* var_read; // heap flag, so that the readers (which take a const module) can set it; set when a reader returns
                    // bytes of them (outside the noise buffer and seed): see sco_u8
} ScoModule;

// The registers a voice's routine staged: the replayer writes them to the hardware one tick later.
typedef struct ScoRegisters {
    uint32_t lc;
    uint16_t len_words;
    uint16_t period;
    uint8_t volume;
} ScoRegisters;

typedef struct Sco2Envelope {
    uint8_t volume;        // $1a2
    uint8_t target;        // $1a6  the level the attack goes to; the release brings it down to `end`
    uint8_t end;           // $1aa  the level the release stops at
    uint8_t attack_step;   // $1ae
    uint8_t release_step;  // $1b2
    uint8_t attack_every;  // $1b6  ticks between steps
    uint8_t release_every; // $1ba
    uint8_t attack_count;  // $1be
    uint8_t release_count; // $1c2
    bool releasing;        // $1ce
    uint8_t limit;         // $1d2  master level the volume is held under
    bool fading;           // $1d6
    uint8_t fade_count;    // $1da
    uint8_t fade_every;    // $1de
} Sco2Envelope;

typedef struct Sco2Voice {
    bool active;                // $14e
    uint8_t hold;               // $14a  further times the next note is played
    uint8_t speed;              // $152
    uint8_t speed_counter;      // $156
    uint8_t length;             // $15e
    uint8_t length_counter;     // $15a
    bool new_note;              // $166
    bool no_retrigger;          // $16a
    uint16_t sequence_pos;      // $16e  position in a wave sequence
    uint16_t sequence_wave;     // $176  wave of the sequence's last record
    uint16_t sequence_before;   // $17e  ... and before it
    uint8_t sequence_transpose; // $186
    uint8_t pattern_pos;        // $18a
    uint8_t stream_pos;         // $18e
    uint8_t transpose;          // $192
    uint8_t instrument_offset;  // $196
    uint8_t pattern_repeat;     // $19a
    uint8_t instrument;         // $19e
    Sco2Envelope envelope;
    uint8_t vibrato_delay; // $1e2
    uint8_t vibrato_shift; // $1e6
    uint16_t vibrato_step; // $1ea
    uint8_t phase;         // $212  vibrato phase, arpeggio delay
    uint8_t cycle;         // $216  vibrato cycle length, arpeggio speed
    bool sliding;          // $1f2
    uint8_t slide_rate;    // $1f6
    uint8_t slide_start;   // $1fa
    uint8_t note;          // $1fe
    uint16_t period;       // $202
    uint16_t note_period;  // $20a
    uint16_t sweep_pos;    // $21a
    uint16_t sweep_lower;  // $222
    uint16_t sweep_upper;  // $22a
    uint8_t sweep_step;    // $232
    bool sweep_down;       // $236
    uint8_t effect;        // $23a  0: none, 1: vibrato, 2: arpeggio
    uint8_t sample;        // $23e
    bool stream_restart;   // $242  the stream loop reset the voice's modifiers
    uint8_t mode;          // $246  low nibble: the wave type
    uint8_t arp_pos;       // $24a
    uint8_t arp_table;     // $24e
    uint16_t chunk_bytes;  // $252
    uint16_t chunk_count;  // $25a
    uint32_t stream_base;  // $27a
    ScoRegisters staged;   // $292
    ScoRegisters written;  // $2d2
    bool played;
} Sco2Voice;

typedef struct Sco2 {
    ScoModule* module;
    Sco2Voice voice[SCO_VOICES];
    bool restart[SCO_VOICES];      // $164
    bool restart_next[SCO_VOICES]; // $162
    uint8_t lookahead[3];          // $266  the stream bytes behind the pattern number
    uint16_t noise_toggle;         // $264  alternates between the two noise buffers (bit 15 set: the second)
    bool song_end;                 // voice 0 took a jump of its stream: the eagleplayer calls SongEnd
    bool unmodeled;
} Sco2;

// A stream word: a command byte and its argument.
typedef struct ScoStreamEntry {
    uint16_t word;
    uint8_t command;
    uint8_t arg;
} ScoStreamEntry;

typedef struct Sco3Slide {
    bool active;     // $86
    uint8_t start;   // $8a  the note the slide starts from
    uint8_t target;  // $8e  the note it ends on
    uint16_t period; // $92
    uint8_t rate;    // $9a
    uint8_t delay;   // $9e
} Sco3Slide;

typedef struct Sco3Vibrato {
    uint8_t phase; // $a2
    uint8_t depth; // $a6
    uint8_t delay; // $aa
    uint8_t shift; // $ae
} Sco3Vibrato;

typedef struct Sco3Sweep {
    uint8_t type;   // $b2  Sco3Wave
    uint16_t pos;   // $b6
    uint16_t lower; // $be
    uint16_t upper; // $c6
    uint8_t step;   // $ce  bit 7: moving down
} Sco3Sweep;

typedef struct Sco3WaveSequence {
    uint8_t id;        // $e6
    uint8_t index;     // $ea
    uint16_t previous; // $ee
    uint16_t current;  // $f6
} Sco3WaveSequence;

typedef struct Sco3Envelope {
    int8_t volume;        // $fe
    int8_t target;        // $102
    uint8_t step;         // $106
    uint8_t interval;     // $10a
    uint8_t counter;      // $10e
    uint8_t delay;        // $112
    int8_t limit;         // $116  master level the volume is held under
    uint8_t fade_counter; // $11a
    uint8_t fade_every;   // $11e  ticks per step of the master fade out (0: none)
} Sco3Envelope;

typedef struct Sco3Stream {
    bool active;          // $122
    uint32_t position;    // $126
    uint32_t end;         // $136
    uint32_t loop;        // $146  0: none
    uint16_t chunk_words; // $156
    uint16_t chunk_step;  // $15e  bytes
    uint32_t silence;     // $166
} Sco3Stream;

typedef struct Sco3Voice {
    bool active;                // $1a
    uint8_t speed;              // $1e
    uint8_t speed_counter;      // $22
    uint8_t length;             // $26
    uint8_t length_counter;     // $2a
    uint16_t pattern_pos;       // $2e
    uint16_t stream_pos;        // $36
    ScoStreamEntry stream_next; // $3e  the stream word behind the pattern's entry
    uint16_t stream_jump;       // $46
    uint8_t pattern_repeat;     // $4e
    int8_t loop_count;          // $52
    uint16_t loop_pos;          // $56
    uint8_t note;               // $5e
    uint16_t period;            // $66
    uint8_t instrument;         // $6e
    uint8_t transpose;          // $72
    uint8_t instrument_offset;  // $76
    uint8_t pitch_mode;         // $7a  a PitchMode, or the number of a pitch sequence mode
    uint8_t pitch_sequence;     // $7e
    uint8_t pitch_pos;          // $82
    Sco3Slide slide;
    Sco3Vibrato vibrato;
    Sco3Sweep sweep;
    uint8_t sample;         // $d2
    uint32_t sample_offset; // $d6
    Sco3WaveSequence waves;
    Sco3Envelope envelope;
    Sco3Stream stream;
    bool no_retrigger;    // $18e
    uint32_t stream_base; // $192  the voice's stream of the song
    ScoRegisters staged;  // $1e2
    ScoRegisters written; // $1a2
    bool played;          // a note has started
} Sco3Voice;

typedef struct Sco3 {
    ScoModule* module;
    Sco3Voice voice[SCO_VOICES];
    bool restart[SCO_VOICES];      // $18a  channels whose DMA the tick restarts
    bool restart_next[SCO_VOICES]; // $18c  ... requested by this tick's routines
    bool song_end;                 // voice 0 took a jump of its stream: the eagleplayer calls SongEnd
    bool unmodeled;
} Sco3;

struct ScoPlayer {
    ScoModule module;
    uint32_t subsong;
    NuadePaulaChannel paula[SCO_VOICES];
    Sco3 sequencer; // the replayer of ScoFamily_Sequencer modules
    Sco2 sampler;   // ... and of ScoFamily_Sampler modules
};

// The eagleplayer's Check2 plus the limits of what this port models: the replayer must be one of the known versions.
bool sco_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `sco_probe` rejects it.
ScoPlayer* sco_create(const uint8_t* data, size_t size);
void sco_destroy(ScoPlayer* player);

uint32_t sco_subsong_count(const ScoPlayer* player);

// The replayer version's name ("Reaxion", ...).
const char* sco_version_name(const ScoPlayer* player);

// (Re)start `subsong` from its beginning; the player drives `voices` (Paula channel n = voice n).
bool sco_start(ScoPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
ScoTickResult sco_tick(ScoPlayer* player, NuadeVoices* voices);

ScoVoiceState sco_voice_state(const ScoPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
ScoPlayer* sco_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
