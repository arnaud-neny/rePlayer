// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FC_TICK_HZ 50
#define FC_VOICES 4

#define FC_PATTERN_ROWS 32
// The lowest period table entries sit this many octaves below C-0.
#define FC_NOTE_OCTAVE_SHIFT (-1)

// Volume sequence bytes stored per sequence, and zero bytes after them for commands that read their arguments.
#define FC_VOL_SEQ_LEN 59
#define FC_VOL_SEQ_PAD 2

typedef struct FcModule FcModule;

// What the display shows. Every voice walks its own sequence list; the row is the one the last pattern step played
// (the counter has moved on). Notes are semitones from C-0 plus FC_NOTE_OCTAVE_SHIFT octaves, the note actually played
// (pattern note + the voice's transpose); the instrument is the volume sequence it selects (-1 if none).
typedef struct FcVoiceState {
    int note;
    int instrument;
    int order;
    int row;
    int speed; // ticks per row
} FcVoiceState;

// One pattern line as written (no transpose); flags are the portamento bits of the info byte (0x80 / 0x40).
typedef struct FcCell {
    int note; // -1 if none
    int instrument;
    int flags;
} FcCell;

typedef struct FcPatternRow {
    uint8_t note;
    uint8_t info;
} FcPatternRow;

typedef struct FcPattern {
    FcPatternRow rows[32];
} FcPattern;

typedef struct FcVoiceSeq {
    uint8_t pattern;
    int8_t transpose;
    int8_t sound_transpose;
} FcVoiceSeq;

typedef struct FcSequence {
    FcVoiceSeq voice_seq[4];
    uint8_t speed;
} FcSequence;

typedef struct FcVolSequence {
    uint8_t speed;
    uint8_t frq_number;
    int8_t vib_speed;
    int8_t vib_depth;
    uint8_t vib_delay;
    uint8_t values[FC_VOL_SEQ_LEN + FC_VOL_SEQ_PAD];
} FcVolSequence;

typedef struct FcMultiSampleEntry {
    int8_t* address;
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
} FcMultiSampleEntry;

typedef struct FcMultiSample {
    FcMultiSampleEntry sample[20];
} FcMultiSample;

typedef struct FcSample {
    int8_t* address;
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
    FcMultiSample* multi;
} FcSample;

typedef struct FcVoiceInfo {
    int8_t pitch_bend_speed;
    uint8_t pitch_bend_time;
    uint16_t song_pos;
    int8_t cur_note;
    uint8_t* volume_seq; // pointer into vol sequence values
    int volume_seq_len;  // length of the volume_seq array
    uint8_t volume_bend_speed;
    uint8_t volume_bend_time;
    uint16_t volume_seq_pos;
    int8_t sound_transpose;
    uint8_t volume_counter;
    uint8_t volume_speed;
    uint8_t vol_sus_counter;
    uint8_t sus_counter;
    int8_t vib_speed;
    int8_t vib_depth;
    int8_t vib_value;
    int8_t vib_delay;
    FcPattern* cur_pattern;
    bool vol_bend_flag;
    bool port_flag;
    uint16_t pattern_pos;
    bool pitch_bend_flag;
    int8_t patt_transpose;
    int8_t transpose;
    int8_t volume;
    uint8_t vib_flag;
    uint8_t portamento;
    uint16_t frequency_seq_start_offset;
    uint16_t frequency_seq_pos;
    uint16_t pitch;

    // What the display shows: the row played by the last fc_new_note and the note started on it.
    uint16_t shown_song_pos;
    uint16_t shown_row;
    int shown_note; // period table index of the last note started, -1 if none
    int shown_inst; // volume sequence number it plays, -1 if none (or silent)
} FcVoiceInfo;

typedef struct FcGlobalPlayingInfo {
    uint16_t re_sp_cnt;
    uint16_t rep_spd;
    uint16_t spd_temp;
    bool aud_temp[4];
} FcGlobalPlayingInfo;

// Paula-style registers of one channel, handed to the voice at the end of a tick.
typedef struct FcChannel {
    NuadeSample head; // started by the last ch_play_sample
    NuadeSample next; // what Paula latches to follow the head
    uint16_t period;
    uint16_t volume; // 0..64
    bool active;     // DMA on
    bool play_pending;
    bool queue_pending;
} FcChannel;

struct FcModule {
    NuadeVoices* voices;

    FcSample samp_info[90]; // 10 samples + 80 wavetables
    int16_t samp_num;
    int16_t wav_num;

    FcSequence* sequences;
    int16_t seq_num;

    FcPattern* patterns;
    int16_t pat_num;

    uint8_t* frq_sequences;
    int frq_sequences_len;

    FcVolSequence* vol_sequences;
    int16_t vol_num; // number of vol sequences (excluding silent at index 0)

    FcGlobalPlayingInfo playing_info;
    FcVoiceInfo voice_data[FC_VOICES];
    FcChannel channels[FC_VOICES];

    bool end_reached;
    uint32_t loop_count; // times the song ran back into a position it had already played
    bool is_smod;        // converted from FC 1.0-1.3

    // Position visit tracking
    uint8_t visited[256];
};

// True when the data carries a Future Composer module mark (FC14, or SMOD that converts).
bool fc_probe(const uint8_t* data, size_t size);

FcModule* fc_create(const uint8_t* data, size_t size);
void fc_destroy(FcModule* module);

int fc_subsong_count(const FcModule* module);
// (Re)start a subsong from the first position; the module drives `voices` from then on.
bool fc_start(FcModule* module, int subsong, NuadeVoices* voices);

// Runs one 50 Hz player tick and updates the voices. True when the song ran back into a position it had already
// played.
bool fc_tick(FcModule* module);

// "Future Composer 1.4" or "Future Composer 1.0-1.3".
const char* fc_format_name(const FcModule* module);

int fc_order_count(const FcModule* module);
// Rows of the longest pattern among the voices at sequence position `order` (a pattern ends at its END mark).
int fc_pattern_rows(const FcModule* module, int order);
FcVoiceState fc_voice_state(const FcModule* module, int voice);
// Line `row` of the pattern `voice` plays at `order`; empty past that voice's own pattern end.
FcCell fc_cell(const FcModule* module, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
FcModule* fc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
