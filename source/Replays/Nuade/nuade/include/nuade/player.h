
// nuade player API: the vtable every player implements, and the types it shares with the host API (nuade.h).
// Optional entries may be 0. (out, cap) fill calls return the count written.
//
// Single-threaded per instance. A player never touches the OS for files: everything goes through NuadeSongSource.io.

#pragma once

#include "io.h"
#include "viz.h"
#include "voice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUADE_PLAYER_API_VERSION 2

// Metadata tags (NuadeMetadataSink.set_tag keys)
#define NUADE_META_TITLE "title"
#define NUADE_META_ARTIST "artist"
#define NUADE_META_FORMAT "format"
#define NUADE_META_TOOL "tool"
#define NUADE_META_MESSAGE "message"

typedef enum NuadeStatus {
    NuadeStatus_Ok = 0,
    NuadeStatus_Unsupported, // no player recognises the data
    NuadeStatus_IoError,     // the song or a file it needs could not be read
    NuadeStatus_InvalidData, // recognised, but broken
    NuadeStatus_OutOfMemory,
    NuadeStatus_InvalidArgument,
} NuadeStatus;

typedef struct NuadeError {
    NuadeStatus status;
    char message[128];
} NuadeError;

typedef enum NuadeProbe {
    NuadeProbe_Supported = 0,
    NuadeProbe_Unsupported,
    NuadeProbe_Unsure, // cannot tell from the data alone; a player that is sure wins
} NuadeProbe;

// The song as a player sees it: always loaded, always with an io.
typedef struct NuadeSongSource {
    const uint8_t* data; // stays valid until the player's close
    size_t size;
    const char* path;  // external files resolve against it; 0 when the song came from memory without a path
    const NuadeIo* io; // never 0
} NuadeSongSource;

typedef struct NuadeSongInfo {
    uint32_t voice_count;
    uint32_t subsong_count;
    uint32_t default_subsong;
    uint32_t timebase_hz; // tick players: the unit of NuadeTickResult.length (709379 = PAL CIA, 50 = VBlank); else 0
    // The voices the display shows (voice_info, snapshots, the row grid), when they are not the mixer's: a player that
    // mixes its own channels into its voices (8 tracks into Paula's 4) gives the number of channels. 0 = the
    // voice_count voices; the framework then sets it to voice_count, so a host can always read it.
    uint32_t display_voices;
} NuadeSongInfo;

typedef struct NuadePlayerOpenResult {
    void* user_data; // the player's state; 0 on failure
    NuadeSongInfo info;
    NuadeError error;
} NuadePlayerOpenResult;

typedef enum NuadeRenderFlags {
    NuadeRenderFlags_None = 0,
    NuadeRenderFlags_Looped = 1, // the song passed its loop point during this call (once per pass)
    NuadeRenderFlags_Ended = 2,  // the song cannot continue; frames after `frames` are not rendered
} NuadeRenderFlags;

typedef enum NuadeTickFlags {
    NuadeTick_None = 0,
    NuadeTick_Looped = 1, // the song passed its loop point during this tick
    NuadeTick_Ended = 2,  // the song cannot continue: nothing is rendered after this tick
} NuadeTickFlags;

typedef struct NuadeTickResult {
    uint32_t length; // until the next tick, in timebase units (exact integer; the framework carries the remainder)
    uint32_t flags;  // NuadeTickFlags
} NuadeTickResult;

typedef struct NuadeRenderResult {
    uint32_t frames; // frames written
    uint32_t flags;  // NuadeRenderFlags
} NuadeRenderResult;

// Metadata: the player pushes it once after open; the framework stores it and serves nuade_meta().

typedef struct NuadeMetadataSink {
    void* ctx;
    void (*set_tag)(void* ctx, const char* key, const char* value);
    void (*add_instrument)(void* ctx, uint32_t index, const char* name);
} NuadeMetadataSink;

typedef struct NuadePlayer {
    uint64_t api_version; // NUADE_PLAYER_API_VERSION
    const char* id;       // short, stable: "sonix"
    const char* name;     // "Sonix Music Driver"
    const char* version;
    const char* extensions; // comma separated, no dots: "snx,smus,tiny"

    NuadeProbe (*probe)(const uint8_t* data, size_t size, const char* filename);
    NuadePlayerOpenResult (*open)(const NuadeSongSource* source);
    void (*close)(void* user_data);
    // (Re)start a subsong from its beginning. Tick players set up their voices through `voices`; a self-rendering
    // player gets 0 there and the output rate in sample_rate (a tick player must not depend on it).
    bool (*start)(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices);

    // Exactly one of tick and render_raw.
    //
    // Tick player: run one step of the song and set the voices through nuade_voice_*. The framework owns the clock and
    // the mixer.
    NuadeTickResult (*tick)(void* user_data, NuadeVoices* voices);

    // Self-rendering player (transitional; the player runs its own mixer): float stereo, interleaved. `voices` is 0, or
    // info.voice_count mono buffers of `frames` floats that receive each voice's output (its left + right
    // contribution), so one voice's buffer equals the song with only that voice audible, summed to mono.
    NuadeRenderResult (*render_raw)(void* user_data, float* stereo, float* const* voices, uint32_t frames);

    void (*metadata)(void* user_data, const NuadeMetadataSink* sink);

    // Display, optional. Read-only: no allocation, no locking, no I/O, no change to the song. The framework calls them
    // right after every tick (every render call for a self-rendering player).
    //
    // position: not valid if the song has no position to report.
    NuadePositionResult (*position)(void* user_data);
    // voice_info: fills up to `cap` voices (the display voices, see NuadeSongInfo.display_voices), in voice order, and
    // returns how many it filled; the rest read as note -1, instrument -1.
    uint32_t (*voice_info)(void* user_data, NuadeVoiceInfo* out, uint32_t cap);
    // events (Events mode): the entries of a voice's stream around `track_pos`: up to `before` entries that precede it,
    // then up to `after` from it on (the first of those is the next one to run). Written oldest first; returns how
    // many. Stateless: any position is valid, so a display can ask for the one in an older snapshot.
    uint32_t (*events)(void* user_data, uint32_t voice, uint32_t track_pos, uint32_t before, uint32_t after,
                       NuadeEvent* out, uint32_t cap);
    // tracker_info (Rows mode): the layout of the pattern at `at` (a position from a snapshot); not valid if there is
    // none. The row count comes from the song data at `at.order`, or from `at.rows` when the player sets that (a count
    // that changes while it plays); stateless, so an older snapshot gets the count it had. For a format whose voices
    // run their own order lists, the host asks with `at.order` = NuadeVoiceInfo.order of the channel.
    NuadeTrackerInfoResult (*tracker_info)(void* user_data, const NuadePosition* at);
    // tracker_cells: the cells of one channel for rows row_lo .. row_hi - 1 of the pattern at `at`, written in row
    // order (at most `cap`); a row outside the pattern reads as all -1. Returns how many it wrote. Stateless, like
    // events. The host picks the rows: usually around NuadePosition.row, or NuadeVoiceInfo.row of that channel.
    uint32_t (*tracker_cells)(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                              int32_t row_hi, NuadePatternCell* out, uint32_t cap);
    // grid_ticks (Rows mode, tick players, instead of tracker_info and tracker_cells): for a song whose voices run on
    // their own timing. Returns the ticks one row of the grid spans for `subsong`, 0 for none; read-only like the
    // other display hooks. The framework scans the subsong (nuade_scan) and answers nuade_tracker_info and
    // nuade_tracker_cells from the voice starts: one pattern (order 0) of all the scanned ticks, a Note and an
    // Instrument column from NuadeVoiceInfo after the tick that started the voice. With no position hook the framework
    // reports the position itself: Rows mode, order 0, row = the tick that ran last / ticks per row (the first tick is
    // tick 0, the stamp the scan gives its starts), tick = the remainder, speed = ticks per row; and it sets
    // NuadeVoiceInfo.order and .row of every voice voice_info returns to that order and row; after a loop the count
    // starts again at row 0. A player sets position only to report something more (a real order). The first
    // nuade_tracker_* call of a subsong runs the scan; see nuade_tracker_info in nuade.h. A player with
    // display voices of its own counts the notes it starts in NuadeVoiceInfo.note_starts: the scan takes the starts
    // from there, not from the mixer.
    uint32_t (*grid_ticks)(void* user_data, uint32_t subsong);

    // Test harness, optional: write the player's per-tick state to `path` ("-" = stderr, "" = off), for A/B against
    // the original. False if the file cannot be opened.
    bool (*dump_state)(void* user_data, const char* path);
} NuadePlayer;

#ifdef __cplusplus
}
#endif
