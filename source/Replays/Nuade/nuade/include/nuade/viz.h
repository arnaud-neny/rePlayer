
// nuade display types: what a player reports about where it is and what each voice is doing, and what the host gets
// back in a snapshot. The player fills NuadePosition and NuadeVoiceInfo; the framework adds what the mixer knows
// (NuadeVoiceView).

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUADE_SNAPSHOT_RING 64  // snapshots nuade_snapshot can reach back over
#define NUADE_SCOPE_FRAMES 4096 // samples nuade_scope can return per voice

#define NUADE_MAX_COLUMNS 8

// Cell values every column shares or that a Note column adds. Real values are >= 0.
#define NUADE_CELL_EMPTY (-1) // nothing in the cell
#define NUADE_NOTE_OFF (-2)   // note off / release
#define NUADE_NOTE_CUT (-3)   // note cut / stop

typedef enum NuadeScrollMode {
    NuadeScroll_None = 0, // the player reports no position
    NuadeScroll_Rows,     // patterns: order / pattern / row
    NuadeScroll_Events,   // one event stream per voice, no rows (Sonix SMUS/SNX); see NuadeVoiceInfo.track_pos
} NuadeScrollMode;

// Where the song is as a whole. A field a format has no notion of is 0.
typedef struct NuadePosition {
    NuadeScrollMode mode;
    uint32_t order;
    uint32_t pattern;
    uint32_t row;
    uint32_t rows; // Rows: rows in the pattern being played when that can change at runtime; 0 = tracker_info knows
    uint32_t tick; // Rows: tick within the row. Events: the song's tick count since it started
    uint32_t speed;
    uint32_t tempo;
} NuadePosition;

typedef struct NuadePositionResult {
    bool valid; // false if the song has no position to report
    NuadePosition position;
} NuadePositionResult;

// What only the player knows about a voice.
typedef struct NuadeVoiceInfo {
    int32_t note;       // the note last started on this voice, -1 if none
    int32_t instrument; // the instrument number the player uses for it, -1 if none
    uint32_t order;     // Rows mode: the order the voice is on (the song's, unless voices run their own order lists)
    uint32_t row;       // Rows mode: the row the voice is on (the song's, unless voices sit on different rows)
    uint32_t track_pos; // Events mode: byte offset of the next event in the voice's track
    char effect[8];     // short text for what the voice is doing besides the note, "" if nothing
    // Notes started on this voice since the subsong started. Read only for a song with display voices of its own
    // (NuadeSongInfo.display_voices): the starts of the row grid.
    uint32_t note_starts;
} NuadeVoiceInfo;

// One entry of a voice's event stream (Events mode), as the score has it.
typedef struct NuadeEvent {
    uint32_t pos;       // byte offset in the song data, the same unit as NuadeVoiceInfo.track_pos
    int32_t note;       // -1 if the entry is not a note
    int32_t instrument; // -1 if the entry does not select one
    char text[24];      // the entry in words: "n60 v90", "wait 10", "inst 3", "end"
} NuadeEvent;

typedef enum NuadeColumnKind {
    NuadeColumn_Note, // semitones from C-0 + NuadeColumnDesc.octave_shift (12 = C-1); the host spells it with
                      // nuade_note_name
    NuadeColumn_Dec,  // decimal number
    NuadeColumn_Hex,  // hexadecimal number
} NuadeColumnKind;

// One column of a pattern cell: what the host draws for value[i] of a NuadePatternCell.
typedef struct NuadeColumnDesc {
    char name[8];
    NuadeColumnKind kind;
    uint8_t digits;      // characters the value takes (Dec, Hex)
    int8_t octave_shift; // Note: added to the octave when spelled, for a format whose lowest notes sit below C-0
} NuadeColumnDesc;

// How the pattern at a position looks (Rows mode).
typedef struct NuadeTrackerInfo {
    uint32_t channels;
    uint32_t rows; // rows in the pattern at that position (NuadePosition.rows when the player sets it)
    uint32_t column_count;
    NuadeColumnDesc columns[NUADE_MAX_COLUMNS];
} NuadeTrackerInfo;

typedef struct NuadeTrackerInfoResult {
    bool valid; // false if there is no pattern at that position
    NuadeTrackerInfo info;
} NuadeTrackerInfoResult;

// One cell: value[i] belongs to NuadeTrackerInfo.columns[i], -1 if the column is empty there.
typedef struct NuadePatternCell {
    int32_t value[NUADE_MAX_COLUMNS];
} NuadePatternCell;

// A voice as the host sees it: the player's view plus the mixer's. A song whose display voices are not the mixer's
// (NuadeSongInfo.display_voices) has the player's view only; the mixer fields are 0.
typedef struct NuadeVoiceView {
    NuadeVoiceInfo info;
    bool active;      // the mixer is playing a sample on the voice
    float pitch_hz;   // source frames per second
    float volume;     // linear 0..1
    float pan;        // -1 left .. 1 right
    float sample_pos; // frames into the current sample
} NuadeVoiceView;

// The song as it was at the start of one tick (a tick player) or one render call (a self-rendering player: the state is
// read after the call, so it can be ahead of `frame` by up to that call).
typedef struct NuadeSnapshot {
    uint64_t frame; // output frame the state belongs to
    NuadePosition position;
    uint32_t
        voice_count; // display voices of the song; the voices array of nuade_snapshot receives min(cap, voice_count)
} NuadeSnapshot;

typedef struct NuadeSnapshotResult {
    bool valid; // false if there is no such snapshot
    NuadeSnapshot snapshot;
} NuadeSnapshotResult;

// Spells a Note column value ("C-4", "F#2", "A--1"), or "---" / "===" / "^^^" for the markers; `out` takes 8 chars.
void nuade_note_name(char out[8], int32_t value, int32_t octave_shift);

#ifdef __cplusplus
}
#endif
