// SPDX-License-Identifier: MIT
// Wally Beben player (Amiga game music), translated from the replayers inside the modules and the UADE eagleplayer
// "WallyBeben" (Wanted Team). A module (".wb") is the composer's four-voice replayer followed by its data; the
// replayer was edited from game to game. This port knows the builds of the corpus and refuses every other file.

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

#define WB_VOICES 4

// The CIA clock: the timebase of WB_TICK_LENGTH.
#define WB_TIMEBASE_HZ 709379
// One replayer tick (DeliTracker's default 50 Hz CIA timer), in 1 / WB_TIMEBASE_HZ seconds.
#define WB_TICK_LENGTH 14187

#define WB_ARP_BUFFER_BYTES 40u

typedef struct WbPlayer WbPlayer;

typedef struct WbTickResult {
    bool looped; // the eagleplayer called SongEnd (every voice reached the end of its list, or the song was replaced)
    bool ended;  // the song stopped or reached something the port does not model
} WbTickResult;

typedef struct WbVoiceState {
    int period;     // the period of the last note, 0 if none
    int instrument; // the instrument record of the last note, -1 if none
} WbVoiceState;

typedef enum WbRole {
    WbRole_Cmd,         // song request: the eagleplayer writes the song number, the replayer clears it
    WbRole_SlideSpeedA, // the low nibble of a slide's parameter: patched into an immediate of the code (A, C)
    WbRole_SlideSpeedB, // ... and into a second one
    WbRole_SlideTarget, // the target note of a slide: patched into an immediate of the code
    WbRole_ZeroRegs,    // 32 zero bytes the C builds load the registers from
    WbRole_Periods,     // word per note, from 0 for no note
    WbRole_PeriodOut,   // four words: the period written to Paula
    WbRole_Voices,      // the voice records
    WbRole_VolumeOut,   // the volume field of the voice records
    WbRole_InstFlags,   // the instrument flags field (inst_flags) of the voice records
    WbRole_VoiceIdx,    // loop counter of the voice loop
    WbRole_Playing,
    WbRole_Tempo,
    WbRole_TempoCtr,
    WbRole_VibLimit,
    WbRole_VibShift,
    WbRole_ArpMask,
    WbRole_StreamPos,
    WbRole_VoiceHw,
    WbRole_SlideDir,
    WbRole_StopMask,
    WbRole_PeriodBase,        // four words
    WbRole_SlideTargetPeriod, // four words
    WbRole_SlideStep,
    WbRole_LoopLc,    // four longs
    WbRole_LoopLen,   // four words
    WbRole_LoopOff,   // four words
    WbRole_StreamPtr, // four longs
    WbRole_SongSeq,   // four longs
    WbRole_SeqPtr,
    WbRole_PatPtr,
    WbRole_VibDelta,
    WbRole_VibAcc,
    WbRole_Voice4,
    WbRole_Voice2,
    WbRole_ArpBuf,   // 40 bytes
    WbRole_Samples,  // long per sample
    WbRole_Silence,  // C builds: the long that points at the silent repeat
    WbRole_Instr,    // 8 bytes per instrument
    WbRole_ArpTable, // 8 bytes
    WbRole_Envs,     // 8 bytes per envelope
    WbRole_Songs,    // 16 bytes per song
    WbRole_Patterns, // long per pattern
    WbRole_SlideSum,
    WbRole_VoiceTop, // header byte: index of the first voice of the voice loop
    WbRole_Count
} WbRole;

typedef enum WbLoopMode {
    WbLoop_SampleOffset, // the repeat registers hold the sample start plus the instrument's loop offset
    WbLoop_FlagSilence,  // ... an instrument with flags 0 repeats on the silent long instead
} WbLoopMode;

typedef struct WbQuirks {
    WbLoopMode loop_mode;
    uint8_t seq_pattern_max; // sequence list entries up to this are patterns; 0xFF restarts the list; others transpose
    uint8_t seq_transpose_add;      // the transposition is the entry plus this
    uint8_t note_undo;              // the one-shot note offset is taken back by this much on the next tick
    bool pattern_index_wraps;       // the pattern number times 4 wraps within a byte
    bool song_command;              // 0xFC in a pattern starts another song (else it is a tempo)
    bool wave_byte_command;         // 0xE0 takes the instrument number from the next byte (else 0xE0..0xEF carry it)
    bool stop_at_pattern_end;       // the voice reading a new event stops its DMA first
    bool song_start_dma_off;        // starting a song switches all channels off
    bool sample_offset;             // bit 1 of an instrument's flags: start the sample at an offset
    bool env_divides;               // the envelope's release point is duration / sustain (else duration >> sustain)
    bool zero_volume_stops;         // an envelope that reaches volume 0 stops the voice (else only below 0)
    bool clears_divide_work;        // the envelope starts with the division register cleared
    bool slide_saves_registers;     // the slide's set-up call keeps the caller's registers
    bool slide_window_uses_seq_pos; // the slide's window compares against the list position, not the duration
    bool slide_window_unsigned;     // the slide's window compares unsigned and does not snap to the target
    bool slide_snaps_to_target;     // inside the window the base period jumps to the target
    bool slide_rotates_step;        // the slide step is rotated instead of divided by the ticks
    bool slide_signed_direction;    // the direction compares the notes signed
    bool vibrato_flag;          // the flag that continues the vibrato after a slide, copied from another voice's fields
    bool vib_flip_at_tempo_ctr; // the vibrato swing turns when its position reaches the tempo counter
    bool vib_limit_keeps_period_hi; // the vibrato limit is the word shifted right by 4: the lookup register's second
                                    // byte enters it
    bool envelope_start_slide;      // bit 3 of an envelope's flags sets a slide flag in another voice's fields
    bool arp_mode_is_mask;          // an envelope's arpeggio sets the voice's mode to the mask (else to 0x7C)
    bool note_bump_from_envelope;   // bit 0 of an envelope's flags raises the first tick of the note by a semitone
    bool period_index_signed_word;  // the period lookup indexes with the register's low word, signed
    bool event_keeps_period_hi;     // an event read does not clear the lookup register first
    bool first_block_ignores_flags; // the first block of a note is the instrument's length whatever its flags
    bool loop_keeps_high_word;      // the repeat address keeps the upper half of the previous voice's address
} WbQuirks;

// The first absolute operand (in code order) that holds a role's address.
typedef struct WbSlotRef {
    uint8_t role;
    uint16_t slot;
} WbSlotRef;

typedef struct WbVersion {
    const char* name;
    uint64_t hash;      // wb_code_hash with the song count masked
    uint64_t partition; // which absolute operands hold equal values, in code order
    uint32_t patches;   // bit n - 1: patch n of the eagleplayer's table finds its code
    WbQuirks quirks;
    const WbSlotRef* slots;
    uint32_t slot_count;
} WbVersion;

typedef struct WbVoiceBlock {
    uint8_t hw[4];            // 0x00 register offset of the voice's Paula channel
    uint8_t arp_start[4];     // 0x04 where the voice's arpeggio list starts in the arpeggio buffer
    uint8_t note[4];          // 0x08
    uint8_t seq_pos[4];       // 0x0C position in the sequence list
    uint8_t duration[4];      // 0x10 ticks of the current note's length (steps)
    uint8_t timer[4];         // 0x14
    uint8_t mode[4];          // 0x18 nonzero: the note slides or arpeggiates
    uint8_t arp_mask[4];      // 0x1C
    uint8_t transpose[4];     // 0x20
    uint8_t slide_target[4];  // 0x24 target note of the slide, 0 for none
    uint8_t slide_param[4];   // 0x28 high nibble: ticks before the end it starts; low nibble: speed
    uint8_t slide_down[4];    // 0x2C
    uint8_t slide_up[4];      // 0x30
    uint8_t note_offset[4];   // 0x34
    uint8_t arp_pos[4];       // 0x38
    uint8_t vib_dir[4];       // 0x3C
    uint8_t vib_pos[4];       // 0x40
    uint8_t vib_delay[4];     // 0x44
    uint8_t vib_delay_cnt[4]; // 0x48
    uint8_t wave[4];          // 0x4C byte offset of the instrument record
    uint8_t env_sel[4];       // 0x50 byte offset of the envelope record
    uint8_t env_target[4];    // 0x54
    uint8_t env_attack[4];    // 0x58
    uint8_t env_release[4];   // 0x5C
    uint8_t volume[4];        // 0x60
    uint8_t dma_bit[4];       // 0x64
    uint8_t vib_param[4];     // 0x68
    uint8_t released[4];      // 0x6C
    uint8_t sustain[4];       // 0x70
    uint8_t spare_74[4];      // 0x74 not used by the replayer
    uint8_t inst_flags[4];    // 0x78
    uint8_t spare_7c[4];      // 0x7C not used by the replayer
    uint8_t vib_on[4];        // 0x80
} WbVoiceBlock;

typedef struct WbBudget {
    uint64_t left;
} WbBudget;

typedef struct WbModule {
    const WbVersion* version;
    uint8_t* image; // nuade_paula_padded_copy of the module: the bytes Paula plays, the tables' source
    uint32_t size;
    uint32_t origin;             // the address the module was assembled at
    bool has[WbRole_Count];      // the build has the role
    uint32_t addr[WbRole_Count]; // module offsets of the roles the build has
    uint32_t song_limit;         // the replayer starts songs 1..song_limit; the eagleplayer offers one less
    uint32_t subsong_count;
    uint32_t code_end;     // the rts in front of the period table
    uint32_t period_count; // words of the period table
    uint32_t pattern_count;
    uint32_t sample_count; // longs of the sample table
    uint32_t* song_seq;    // song_limit - 1 songs of 4 sequence lists: module offsets
    uint32_t* pattern_ptr; // module offsets
    uint32_t* sample_ptr;  // module offsets
    uint32_t silence_ptr;  // WbLoop_FlagSilence builds
    uint32_t voice_bytes;  // size of the voice records: the last field (0x80) is only there in builds that use it
    uint32_t var_lo;       // the replayer's variables lie in [var_lo, var_hi)
    uint32_t var_hi;
} WbModule;

// What the proof learns while it plays one voice alone (see wb_replay_prove): whether the voice read or wrote anything
// another voice could change, and the arpeggio buffer bytes it used.
typedef struct WbCoupling {
    uint8_t only_voice;   // the voice that is ticked
    int16_t tempo_value;  // the tempo the voice's events set (all alike), -1 for none
    uint32_t tick;        // ticks played so far
    uint32_t tempo_tick;  // the tick of the first tempo event
    bool period_only;     // the period lookups now going on feed the output period and nothing else
    bool live_read;       // ... and one of them read a variable
    bool coupled;         // the voice depends on, or changes, state shared with the other voices
    bool period_hi_own;   // the voice's tick has written the lookup register's second byte
    bool divide_hi_own;   // ... the upper bytes of the division register
    bool slide_index_own; // ... the upper bytes of the slide's index register
    uint64_t arp_read;    // bit n: arp_buf[n] was read, or written, by the voice
    uint64_t arp_written;
} WbCoupling;

typedef struct WbReplay {
    const WbModule* module;
    NuadePaulaChannel* paula; // WB_VOICES channels
    WbCoupling* track;        // set by the proof, null when playing
    WbBudget* budget;         // the proof's work budget, null when playing
    bool abstract_entry0;     // the proof: the first sample table entry is only used by silent notes
    uint32_t* sample_ptr;     // the sample table (the replayer writes into it), module->sample_count entries
    WbVoiceBlock vb;
    uint16_t period_out[4];
    uint16_t period_base[4];
    uint16_t slide_target_period[4];
    uint16_t slide_step;
    uint32_t loop_lc[4];
    bool loop_from_entry0[4];   // the repeat address was taken from the first sample table entry
    bool base_from_variable[4]; // the base period was read from the variables (a note past the period table)
    bool lc_outside[4];         // the sample address latched for the voice lies in memory the port does not hold
    bool lc_from_entry0[4];     // ... and was taken from the first sample table entry
    uint16_t loop_len[4];
    uint16_t loop_off[4];
    uint32_t stream_ptr[4]; // module offset of the next event, WB_ZERO_STREAM, or WB_BAD_POINTER
    uint32_t song_seq[4];
    uint32_t seq_ptr; // the sequence list being read
    uint32_t pat_ptr; // the pattern being read
    uint32_t voice4;  // the current voice times 4 and times 2
    uint32_t voice2;
    uint16_t vib_delta;
    uint16_t vib_acc;
    uint8_t arp_buf[WB_ARP_BUFFER_BYTES];
    uint8_t cmd;
    uint8_t playing;
    uint8_t tempo;
    uint8_t tempo_ctr;
    uint8_t vib_limit;
    uint8_t vib_shift;
    uint8_t arp_mask;
    uint8_t stream_pos;
    uint8_t voice_hw;
    uint8_t slide_dir;
    bool voice_stopped[WB_VOICES]; // the voice's DMA is switched off at the end of the tick (the bits of the stop mask)
    uint8_t voice_idx;
    uint8_t slide_target; // the slide's target note and speeds: immediates of the code in some builds
    uint8_t slide_speed_a;
    uint8_t slide_speed_b;
    uint8_t slide_sum;
    uint32_t pattern_entry; // byte offset in the pattern table of the pattern the list read last
    uint8_t period_hi;      // second byte of the register the period lookups work in
    uint32_t divide_work;   // the register the envelope and the slide divide in: its upper half is the last remainder
    // the register the slide set-up indexes the period table with (its upper bytes carry over)
    uint32_t slide_index_reg;
    uint8_t voice_top;
    // the eagleplayer's bookkeeping
    bool song_end_armed;        // SongEnd is called by the volume reset only after the first one
    bool voice_open[WB_VOICES]; // the voice has not yet reached the end of its list (the marker "WTWT")
    // results of a tick
    bool song_end;
    bool stopped;
    bool aborted; // the tick was left through a nested dispatch (stop or song command)
    bool fault;
    uint32_t depth;
    // for the display
    int last_period[WB_VOICES];
    int last_instrument[WB_VOICES];
} WbReplay;

struct WbPlayer {
    WbModule module;
    NuadePaulaChannel paula[WB_VOICES];
    uint32_t* sample_ptr; // the replayer's working copy of the sample table
    WbReplay replay;
};

// The eagleplayer's Check2 plus the limits of what this port models: the replayer must be one of the known builds.
bool wb_probe(const uint8_t* data, size_t size);

// Copies and parses the module; null when `wb_probe` rejects it.
WbPlayer* wb_create(const uint8_t* data, size_t size);
void wb_destroy(WbPlayer* player);

uint32_t wb_subsong_count(const WbPlayer* player);

// The replayer build's name.
const char* wb_version_name(const WbPlayer* player);

// (Re)start `subsong` from its beginning; the player drives `voices` (Paula channel n = voice n).
bool wb_start(WbPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one replayer tick and hand the Paula registers it wrote to the voices.
WbTickResult wb_tick(WbPlayer* player, NuadeVoices* voices);

WbVoiceState wb_voice_state(const WbPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
WbPlayer* wb_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
