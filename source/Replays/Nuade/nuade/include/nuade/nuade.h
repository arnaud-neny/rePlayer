
// nuade host API: play any song through any compiled-in player without knowing which one.
// Single-threaded per song.

#pragma once

#include "player.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NuadeSong NuadeSong;

// Songs

typedef struct NuadeSongResult {
    NuadeSong* song; // 0 on failure
    NuadeError error;
} NuadeSongResult;

typedef struct NuadeOpenParams {
    const char* path; // required unless data is set; external files (instruments, samples) resolve against it
    const void* data; // the song file; 0 = read `path` through `io`. Copied, may be freed after the call.
    size_t size;
    const NuadeIo* io;         // 0 = local files (nuade_io_default)
    const NuadePlayer* player; // 0 = probe every compiled-in player
} NuadeOpenParams;

typedef struct NuadeDuration {
    uint64_t frames; // up to the first loop point, or the end of the song
    bool looped;     // true: the song loops (frames = one pass); false: it ends
    bool capped;     // true: neither happened within `max_frames`
} NuadeDuration;

// Silent scan
//
// A dry run of a tick player that records where each voice starts a sample, with no audio rendered. It runs on a
// scratch copy of the song, so the caller's song is not touched; the copy has the io the song was opened with.
//
// A start is a nuade_voice_play, or a nuade_voice_queue on a voice that was idle (it starts at once). For a song with
// display voices of its own (NuadeSongInfo.display_voices) it is a note the player counts in NuadeVoiceInfo.note_starts
// instead, on a display voice, stamped with the frame its tick starts at.

typedef struct NuadeScanEvent {
    uint32_t tick;      // ticks run before it: 0 for the player's start and its first tick, then 1, 2, ...
    uint64_t frame;     // output frame at the rate given to nuade_scan
    uint32_t voice;     // the voice that started a sample
    int32_t note;       // NuadeVoiceInfo.note after that tick, -1 if the player reports none
    int32_t instrument; // NuadeVoiceInfo.instrument after that tick, -1 if none
} NuadeScanEvent;

typedef struct NuadeScan {
    NuadeScanEvent* events; // in call order
    uint32_t count;
    uint32_t ticks;  // ticks run; when `looped`, the last one is the tick that reported the loop
    uint64_t frames; // output frames up to the start of the last tick
    bool looped;     // the scan stopped at the first loop point (the events of that last tick are included)
    bool ended;      // the song ended before looping
    bool truncated;  // neither happened within `max_ticks`
} NuadeScan;

// Registry: the players compiled in (CMake NUADE_PLAYERS).

uint32_t nuade_player_count(void);
const NuadePlayer* nuade_player_get(uint32_t index);
const NuadePlayer* nuade_player_find(const char* id);

// Songs

// Open a song file from local disk.
NuadeSongResult nuade_open(const char* path);
// Open a song already in memory (copied). `path` may be 0, but then a format that needs external files (instrument
// directories, separate sample files) opens without them.
NuadeSongResult nuade_open_memory(const void* data, size_t size, const char* path);
// Open with a custom io, a forced player or both. nuade_open and nuade_open_memory call this.
NuadeSongResult nuade_open_ex(const NuadeOpenParams* params);
void nuade_close(NuadeSong* song);

const NuadeSongInfo* nuade_info(const NuadeSong* song);
const NuadePlayer* nuade_player(const NuadeSong* song);
void nuade_pan(NuadeSong* song, float* old_pan, float* new_pan); // rePlayer

// The player's own state, for custom displays. Use the player's typed accessor (e.g. sonix_from_nuade) instead of
// casting this.
void* nuade_player_data(NuadeSong* song);

// (Re)start a subsong from its beginning at the given output rate.
bool nuade_start(NuadeSong* song, uint32_t subsong, uint32_t sample_rate);

// Render `frames` of float stereo, interleaved. `voices`: 0, or nuade_info()->voice_count mono buffers of `frames`
// floats that receive each voice's output (see NuadePlayer.render_raw).
NuadeRenderResult nuade_render(NuadeSong* song, float* stereo, float* const* voices, uint32_t frames);

// Frames rendered since nuade_start (or the last nuade_seek).
uint64_t nuade_position(const NuadeSong* song);

// Move to `frame` by restarting and rendering without output (a backward seek restarts first). Returns false if the
// song ended before reaching it; position is then where it ended.
bool nuade_seek(NuadeSong* song, uint64_t frame);

// Dry run of a subsong at `sample_rate`: render until it loops, ends or `max_frames` is reached. Leaves the song
// restarted at the beginning of that subsong. Returns false on a bad subsong or rate.
bool nuade_duration(NuadeSong* song, uint32_t subsong, uint32_t sample_rate, uint64_t max_frames, NuadeDuration* out);

// Silent scan (see the types above)

// Scan a subsong of a tick player until it loops, ends or `max_ticks` ticks have run. 0 for a self-rendering player, a
// bad subsong or rate, or no memory. Free the result with nuade_scan_free.
NuadeScan* nuade_scan(const NuadeSong* song, uint32_t subsong, uint32_t sample_rate, uint32_t max_ticks);
void nuade_scan_free(NuadeScan* scan);

// Display
//
// The framework keeps the last NUADE_SNAPSHOT_RING ticks (render calls, for a self-rendering player) of position and
// voice state, each stamped with its output frame. A host that knows the frame it is hearing asks for that one, so the
// display never runs ahead of the audio.
//
// Threads: a NuadeSong is single-threaded. nuade_snapshot, nuade_events, nuade_tracker_info, nuade_tracker_cells,
// nuade_vu and nuade_scope read the song state that nuade_render writes, so a host that renders on an audio thread and
// draws on another one has to hold one lock around both (or copy what it needs under it). The calls do not lock.
//
// Timing: a tick player's snapshot is taken at the start of its tick, so it is exact to the tick. A self-rendering
// player's is taken after the whole render call, so its position can be ahead of the stamped `frame` by up to that
// call; render in small blocks when the display must be tight.

// The newest snapshot with frame <= `frame`; `voices` receives min(cap, display_voices) entries, 0 to skip them. Not
// valid if there is none (before the first tick, or older than the ring remembers).
NuadeSnapshotResult nuade_snapshot(const NuadeSong* song, uint64_t frame, NuadeVoiceView* voices, uint32_t cap);

// The entries of a voice's event stream around a track_pos taken from a snapshot (see NuadePlayer.events). 0 if the
// player has none.
uint32_t nuade_events(NuadeSong* song, uint32_t voice, uint32_t track_pos, uint32_t before, uint32_t after,
                      NuadeEvent* out, uint32_t cap);

// The pattern layout and cells at a position taken from a snapshot (see NuadePlayer.tracker_info, tracker_cells). Not
// valid / 0 if the player has none.
//
// A player with NuadePlayer.grid_ticks gets them from a silent scan of the subsong (at most 360000 ticks). The first
// call after a (re)start into another subsong runs the scan, which takes as long as running the song's logic once, and
// the host holds its lock meanwhile as for any of these calls; later calls read the cached result. The cache is dropped
// at nuade_close.
NuadeTrackerInfoResult nuade_tracker_info(NuadeSong* song, const NuadePosition* at);
uint32_t nuade_tracker_cells(NuadeSong* song, const NuadePosition* at, uint32_t channel, int32_t row_lo, int32_t row_hi,
                             NuadePatternCell* out, uint32_t cap);

// Peak |output| of each voice since the previous nuade_vu call, 0..1; fills min(cap, voice_count) entries and returns
// that count. Meters run from the first call on (that call returns zeros), and only for renders after it.
uint32_t nuade_vu(NuadeSong* song, float* out, uint32_t cap);

// The last `cap` output samples of a voice (left + right, mono), oldest first, from at most the last NUADE_SCOPE_FRAMES
// rendered after the first nuade_vu or nuade_scope call. Returns how many were written.
uint32_t nuade_scope(NuadeSong* song, uint32_t voice, float* out, uint32_t cap);

// Settings: they apply to every player and take effect from the next nuade_render.

// Loop points to play through before the song ends: 0 (the default) plays on through every loop point; N > 0 ends the
// song (NuadeRenderFlags_Ended) at its Nth loop point. A self-rendering player ends within a few ms of that point: it
// is rendered in short slices, and the slice that reaches it is the last.
void nuade_set_repeat_count(NuadeSong* song, uint32_t count);

// Stereo width, 0..1: 1 (the default) keeps each voice's pan, the Amiga's hard left/right for most players; 0 is mono.
// Returns false (and changes nothing) outside 0..1.
bool nuade_set_stereo_separation(NuadeSong* song, float separation);

// Metadata

// Value of a NUADE_META_* tag, 0 if the player did not set it.
const char* nuade_meta(const NuadeSong* song, const char* key);
uint32_t nuade_instrument_count(const NuadeSong* song);
// Name of the n-th instrument the player reported (in the order reported) and its format index.
const char* nuade_instrument_name(const NuadeSong* song, uint32_t n, uint32_t* index);

#ifdef __cplusplus
}
#endif
