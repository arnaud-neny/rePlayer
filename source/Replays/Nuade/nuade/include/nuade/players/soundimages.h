// SPDX-License-Identifier: MIT
// Sound Images (Tony Williams), translated from the UADE eagleplayer "SoundImages". The eagleplayer carries no
// replayer: every module is a 68k program, a game's sound driver, that the eagleplayer starts and calls once per
// tick. The driver ran in several versions; this port models the ones listed in players/soundimages/si_images.c and
// refuses every other module, which UADE then plays. The module layout and the driver are described in
// players/soundimages/.

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

#define SI_VOICES 4

// The eagleplayer's file name prefix (the files are also named with it as a suffix).
#define SI_EXTENSION "tw"

// The CIA clock: the timebase of the tick length.
#define SI_TIMEBASE_HZ 709379

#define SI_MAX_PATCHED 16u // the patch sites, the rte, the song count immediate

typedef struct SiPlayer SiPlayer;

typedef enum SiStatus {
    SiStatus_Ok = 0,
    SiStatus_InvalidData,
    SiStatus_OutOfMemory,
} SiStatus;

typedef struct SiCreateResult {
    SiPlayer* player;
    SiStatus status;
} SiCreateResult;

typedef struct SiTickResult {
    uint32_t length; // this tick's length in 1 / SI_TIMEBASE_HZ seconds
    bool looped;     // the eagleplayer called SongEnd: all voices have reached the end of their lists
    bool ended;      // the driver stopped the song, or the song left the data the port models
} SiTickResult;

// What voice (0-based) last started a note: the period (0 if none yet) and the sample number.
typedef struct SiVoiceState {
    uint32_t period;
    int instrument;
} SiVoiceState;

// The operands of a segment that are not part of the code's identity: they hold an address, which is checked against
// where the image says it points.
typedef enum SiHoleKind {
    SiHole_Branch8,   // the low byte of an opcode word: the displacement of a short branch
    SiHole_Branch16,  // the displacement word of a branch
    SiHole_PcRel16,   // the displacement word of an operand relative to the program counter
    SiHole_PcIndex8,  // the low byte of the extension word of an operand relative to the program counter and a register
    SiHole_Parameter, // the low byte of an immediate that is a count of the module (a song count): no target
} SiHoleKind;

typedef struct SiSegment {
    uint16_t words;
    uint64_t hash; // of the words, the holes zeroed
} SiSegment;

typedef struct SiHole {
    uint8_t segment; // the segment of the operand
    uint16_t word;   // its word in the segment
    SiHoleKind kind;
    int8_t to_segment; // the segment the target is counted from, or SI_TARGET_FREE
    int32_t to_offset; // the target's byte offset from the start of that segment
} SiHole;

// A place in the module: a byte offset from the start of a segment.
typedef struct SiRef {
    int8_t segment;
    int32_t offset;
} SiRef;

// The driver's commands, the bytes $80 and up that a sequence holds. The module dispatches on the byte through a table
// of offsets; the table of an image names what each entry does.
typedef enum SiCommand {
    SiCommand_NextPattern,       // the next sequence of the voice's list
    SiCommand_NoteStepDown,      // the note falls a semitone on every tempo tick
    SiCommand_NoteStepUp,        // the note rises a semitone on every tempo tick
    SiCommand_JumpList,          // the voice's list jumps to the offset the next word gives
    SiCommand_Stop,              // all voices off, the song ends
    SiCommand_Release,           // the release length and speed
    SiCommand_EndNote,           // ends the sequence step
    SiCommand_Silence,           // the volume 0 and ends the step
    SiCommand_Transpose,         // the voice's transposition
    SiCommand_Vibrato,           // delay, depth and rate of the vibrato
    SiCommand_Bend,              // an offset of the period and a slide that ends it
    SiCommand_BendOff,           // the bend off
    SiCommand_SlideOn,           // a slide of the period
    SiCommand_SlideOnRelease,    // a slide that starts when the release does
    SiCommand_SlideOnReleaseOff, // that off
    SiCommand_HoldOn,            // the next notes do not restart the sample
    SiCommand_HoldOff,           // that off
    SiCommand_ArpeggioOff,       // the arpeggio off
    SiCommand_VolumeDown,        // the voice's volume one lower
    SiCommand_VolumeUp,          // the voice's volume one higher
    SiCommand_Fade,              // the master fade out
    SiCommand_VoiceOff,          // the fourth voice's DMA off
    SiCommand_VoiceOffOwn,       // the voice's own DMA off, its volume 0, and the voice out of the song
    SiCommand_VoiceOffOwnSilent, // that, and the volume register 0
} SiCommand;

typedef struct SiTableEntry {
    SiCommand command;
    uint8_t argument;  // the fade out's speed
    int8_t to_segment; // where the entry must point
    int32_t to_offset;
} SiTableEntry;

// What the tick does before its work, in the versions that have more than the common driver.
typedef enum SiPrologue {
    SiPrologue_None,
    SiPrologue_PeriodOverrideSong1,         // song 1: the first voice's period is the header's word at 8
    SiPrologue_PeriodOverrideNotSongs2And4, // every song but 2 and 4
} SiPrologue;

// A place in the module that the eagleplayer's patch table matches (the pattern number is the entry of its table).
typedef struct SiPatchSite {
    uint8_t pattern;
    SiRef at;
} SiPatchSite;

typedef struct SiImage {
    const char* name;      // the files of the corpus that have this driver
    uint32_t first_offset; // where the first segment starts
    const SiSegment* segments;
    size_t segment_count;
    const SiHole* holes;
    size_t hole_count;
    const SiTableEntry* commands; // the table the command bytes $80, $81 ... index
    size_t command_count;
    SiRef record[SI_VOICES];
    SiRef volume_table;
    SiRef period_table;
    SiRef sample_table;
    SiRef song_table;
    SiRef arpeggio_table;
    SiRef command_table;
    // What the eagleplayer's InitPlayer and EP_ModuleChange see in the module (player.asm): the driver's tick entry
    // (the target of the bra.w at 0), the first bsr.w behind it (the song start), the first rte word of the module (the
    // audio handler's), and the places where its patch table matches in the first $A5A bytes.
    SiRef entry;
    SiRef song_call;
    SiRef rte;
    const SiPatchSite* patch_sites;
    size_t patch_site_count;
    bool song_start_needs_word; // the song start returns while the header word at 8 is 0
    SiRef end_command;          // the byte that runs when a voice's list ends
    // The driver of the older games gives each voice an active flag; the newer one runs the voices while a song plays.
    bool voice_flags;
    SiPrologue prologue;
    uint32_t record_bytes;         // SI_RECORD_BYTES_WIDE or SI_RECORD_BYTES_NARROW
    bool arpeggio_end_masked;      // the pattern byte with the top bit set restarts the pattern and plays as the byte
                                   // without it
    bool song_start_stops_dma;     // the song start switches the DMA of all voices off
    bool song_start_clears_voices; // the song start puts the voices without a list out of the song
    // The first byte of each class of command: all bytes from there on to the next class are one command with the
    // byte's distance from the base as its argument.
    uint8_t duration_class;
    uint8_t sample_class;
    uint8_t envelope_class;
    uint8_t volume_class;
    uint8_t arpeggio_class;
} SiImage;

// A range of the module: [start, end).
typedef struct SiRange {
    uint32_t start;
    uint32_t end;
} SiRange;

// Where the data of the driver sits in a module of an image.
typedef struct SiLayout {
    uint32_t record[SI_VOICES];
    uint32_t volume_table;
    uint32_t period_table;
    uint32_t sample_table;
    uint32_t sample_count;
    uint32_t song_table;
    uint32_t arpeggio_table;
    uint32_t command_table;
    uint32_t song_count;
    uint8_t end_command; // the byte that runs when a voice's list ends
    // The words the eagleplayer rewrites in the code: the driver reads the patched words, the port the file's, so no
    // data read may touch them.
    SiRange patched[SI_MAX_PATCHED];
    uint32_t patched_count;
    uint32_t record_bytes;
    bool start_pending[SI_VOICES]; // the voices that the header's DMACON word starts at the first tick
} SiLayout;

typedef struct SiModule {
    uint8_t* memory; // the module with NUADE_PAULA_MAX_SAMPLE_BYTES of zeros behind it
    size_t size;
    const SiImage* image;
    SiLayout layout;
} SiModule;

typedef struct SiVoice {
    NuadePaulaChannel paula;
    // The voice's record
    uint32_t sample;            // $00 offset of the sample in the module, 0 before the first sample command
    uint16_t length_words;      // $04
    uint16_t period;            // $06
    uint8_t volume_high;        // $08
    uint8_t volume;             // $09
    int64_t list;               // $0e the voice's list of sequences, an offset in the module
    int64_t sequence;           // $12
    bool arpeggio;              // $16 bit 0
    bool slide;                 //     bit 1
    bool bend;                  //     bit 2
    bool vibrato_up;            //     bit 3
    bool hold;                  //     bit 4
    bool slide_on_release;      //     bit 5
    uint8_t duration;           // $17
    uint8_t note;               // $18
    uint8_t note_step;          // $19
    uint32_t loop;              // $1a the repeat the audio interrupt writes, 0 before the first sample command
    uint16_t loop_length_words; // $20
    uint8_t volume_base;        // $24
    uint8_t attack_step;        // $25
    uint8_t attack_count;       // $26
    uint8_t decay_step;         // $27
    uint8_t decay_count;        // $28
    uint8_t release_step;       // $29
    uint8_t release_start;      // $2a
    bool release_on;            // $2b
    uint8_t attack_left;        // $2c
    uint8_t decay_left;         // $2d
    uint8_t vibrato_depth;      // $2e
    uint8_t vibrato_rate;       // $2f
    uint8_t vibrato_left;       // $30
    uint8_t slide_left;         // $31
    uint16_t slide_step;        // $32
    uint16_t bend_offset;       // $34
    uint8_t bend_ticks;         // $36
    uint8_t note_length;        // $37
    uint8_t attack_wait;        // $3a
    uint8_t attack_rate;        // $3b
    uint8_t decay_wait;         // $3c
    uint8_t decay_rate;         // $3d
    uint8_t release_wait;       // $3e
    uint8_t release_rate;       // $3f
    uint8_t vibrato_delay;      // $40
    uint8_t arpeggio_index;     // $41
    int64_t arpeggio_pattern;   // $42 the offset of the arpeggio's notes
    uint8_t transpose;          // $46
    // What the voice's hardware does between ticks
    bool start_pending; // a note started: at the next tick the DMA comes on and the audio interrupt with it
    bool list_alive;    // the eagleplayer's per-voice mask: the voice has not reached the end of its list
    bool active;        // $47 (the driver with voice flags): the song uses the voice
    uint32_t shown_period;
    int shown_instrument;
} SiVoice;

// The driver's variables behind the header
typedef struct SiState {
    uint8_t request;        // $0 the song to start (1-based), 0 for none
    uint8_t song;           // $1 the song that plays, 0 when none
    uint8_t fade_speed;     // $4
    uint8_t fade_counter;   // $5
    uint16_t master_volume; // $6
    uint8_t tempo;          // $12
    uint8_t tempo_counter;  // $13
    uint8_t latched;        // $14
    uint16_t word8;         // $8 the song start of one version tests it, two tick prologues force it on voice 0
} SiState;

struct SiPlayer {
    SiModule module;
    SiState state;
    SiVoice voices[SI_VOICES];
    bool broken; // the song left the data the port models
    bool looped; // this tick called the eagleplayer's SongEnd
};

// The eagleplayer's module check plus the proof that the module's driver is one the port models.
bool si_probe(const uint8_t* data, size_t size);

SiCreateResult si_create(const uint8_t* data, size_t size);
void si_destroy(SiPlayer* player);

// The number of songs the eagleplayer's SubSongRange reports (at least 1).
uint32_t si_song_count(const SiPlayer* player);

// (Re)start a song (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool si_start(SiPlayer* player, uint32_t song, NuadeVoices* voices);

SiTickResult si_tick(SiPlayer* player, NuadeVoices* voices);

// The driver's name: "Sound Images".
const char* si_format_name(const SiPlayer* player);
SiVoiceState si_voice_state(const SiPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SiPlayer* si_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
