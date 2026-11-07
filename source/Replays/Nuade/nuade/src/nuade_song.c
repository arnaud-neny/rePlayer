// NuadeSong: opens a song with the right player and forwards playback, settings and metadata to it.

#include "nuade_test.h"

#include "nuade_base.h"
#include "nuade_grid.h"
#include "nuade_mixer.h"
#include "nuade_path.h"
#include "nuade_scheduler.h"
#include "nuade_trace.h"
#include "nuade_utils.h"
#include "nuade_viz.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Thread_local const NuadePlayer* g_player = nullptr; // rePlayer

typedef struct NuadeTag {
    char* key;
    char* value;
} NuadeTag;

typedef struct NuadeInstrument {
    u32 index;
    char* name;
} NuadeInstrument;

struct NuadeSong {
    const NuadePlayer* player;
    void* user_data;
    NuadeSongInfo info;
    u8* data; // owned copy of the song file
    size_t size;
    char* path;        // owned copy, may be nullptr
    const NuadeIo* io; // never nullptr; must outlive the song
    NuadeTag* tags;
    u32 tag_count;
    NuadeInstrument* instruments;
    u32 instrument_count;
    u32 instrument_cap;

    // Tick players only: the framework's mixer and clock.
    NuadeVoices* voices;
    NuadeScheduler scheduler;
    u32 frames_until_tick;
    u32 ticks_run; // ticks run since nuade_start or the last loop; the scan stamp of the last one is ticks_run - 1
    bool ended;

    // Display: the snapshot ring, and the per-voice buffers the meters read when the caller asked for none.
    // own_display: the player's display voices are not the mixer's (NuadeSongInfo.display_voices).
    NuadeViz* viz;
    bool own_display;
    float** meter_voices;
    u32 meter_frames;

    u32 subsong;
    u32 sample_rate;
    u64 position;
    u32 loops;
    u32 repeat_count;
    f32 stereo_separation;

    // The scan behind the row grid (NuadePlayer.grid_ticks) and the subsong it was made for.
    NuadeScan* grid_scan;
    u32 grid_subsong;

    // Test hooks: the caller's voice observer and the event trace both watch the mixer.
    NuadeVoiceObserver observer;
    void* observer_user;
    NuadeTrace* trace;
};

// The rate of the scan behind the row grid; only its tick count is used.
#define GRID_SCAN_RATE 48000

static NuadeSongResult fail(NuadeStatus status, const char* fmt, ...) {
    NuadeSongResult result = { 0 };
    result.error.status = status;
    va_list args;
    va_start(args, fmt);
    vsnprintf(result.error.message, sizeof(result.error.message), fmt, args);
    va_end(args);
    return result;
}

// Metadata sink: the player pushes tags and instrument names once after open; the song keeps copies.

static void sink_set_tag(void* ctx, const char* key, const char* value) {
    NuadeSong* song = (NuadeSong*)ctx;
    if (key == nullptr || value == nullptr) {
        return;
    }
    for (u32 i = 0; i < song->tag_count; i++) {
        if (strcmp(song->tags[i].key, key) == 0) {
            char* v = nuade_strdup(value);
            if (v != nullptr) {
                free(song->tags[i].value);
                song->tags[i].value = v;
            }
            return;
        }
    }
    NuadeTag* tags = (NuadeTag*)realloc(song->tags, (song->tag_count + 1) * sizeof(NuadeTag));
    if (tags == nullptr) {
        return;
    }
    song->tags = tags;
    NuadeTag tag = { nuade_strdup(key), nuade_strdup(value) };
    if (tag.key == nullptr || tag.value == nullptr) {
        free(tag.key);
        free(tag.value);
        return;
    }
    song->tags[song->tag_count++] = tag;
}

static void sink_add_instrument(void* ctx, uint32_t index, const char* name) {
    NuadeSong* song = (NuadeSong*)ctx;
    if (name == nullptr) {
        return;
    }
    if (song->instrument_count == song->instrument_cap) {
        const u32 cap = song->instrument_cap ? song->instrument_cap * 2 : 64;
        NuadeInstrument* list = (NuadeInstrument*)realloc(song->instruments, cap * sizeof(NuadeInstrument));
        if (list == nullptr) {
            return;
        }
        song->instruments = list;
        song->instrument_cap = cap;
    }
    char* copy = nuade_strdup(name);
    if (copy == nullptr) {
        return;
    }
    song->instruments[song->instrument_count++] = (NuadeInstrument) { .index = index, .name = copy };
}

static void free_meter_buffers(NuadeSong* song) {
    if (song->meter_voices != nullptr) {
        free(song->meter_voices[0]);
        free(song->meter_voices);
    }
    song->meter_voices = nullptr;
    song->meter_frames = 0;
}

static void free_song(NuadeSong* song) {
    for (u32 i = 0; i < song->tag_count; i++) {
        free(song->tags[i].key);
        free(song->tags[i].value);
    }
    for (u32 i = 0; i < song->instrument_count; i++) {
        free(song->instruments[i].name);
    }
    nuade_scan_free(song->grid_scan);
    nuade_trace_destroy(song->trace);
    free_meter_buffers(song);
    nuade_viz_destroy(song->viz);
    nuade_mixer_destroy(song->voices);
    free(song->tags);
    free(song->instruments);
    free(song->path);
    free(song->data);
    free(song);
}

// True when the file name carries one of the player's extensions, as a "ext.name" prefix or a ".ext" suffix.
static bool name_matches(const NuadePlayer* player, const char* path) {
    if (player->extensions == nullptr || path == nullptr) {
        return false;
    }
    const char* ext = player->extensions;
    while (*ext != '\0') {
        const char* end = strchr(ext, ',');
        const size_t len = end ? (size_t)(end - ext) : strlen(ext);
        char name[32];
        if (len > 0 && len < sizeof(name)) {
            memcpy(name, ext, len);
            name[len] = '\0';
            if (nuade_path_has_ext(path, name)) {
                return true;
            }
        }
        ext += len + (end ? 1 : 0);
    }
    return false;
}

// Like UADE: a player whose extension matches the file name and accepts the data wins first; else the first player
// that is sure from the data; else the first that is unsure.
static const NuadePlayer* probe_players(const u8* data, size_t size, const char* path) {
    const NuadePlayer* sure = nullptr;
    const NuadePlayer* unsure = nullptr;
    for (u32 i = 0; i < nuade_player_count(); i++) {
        const NuadePlayer* player = g_player = nuade_player_get(i); // rePlayer
        const NuadeProbe probe = player->probe(data, size, path);
        if (probe == NuadeProbe_Unsupported) {
            continue;
        }
        if (name_matches(player, path)) {
            return player;
        }
        if (probe == NuadeProbe_Supported && sure == nullptr) {
            sure = player;
        } else if (probe == NuadeProbe_Unsure && unsure == nullptr) {
            unsure = player;
        }
    }
    return sure ? sure : unsure;
}

// Copy the song into memory the song owns: from params->data, or read through io.
static bool load_data(NuadeSong* song, const NuadeOpenParams* params, const NuadeIo* io) {
    if (params->data != nullptr) {
        song->data = (u8*)malloc(params->size ? params->size : 1);
        if (song->data == nullptr) {
            return false;
        }
        memcpy(song->data, params->data, params->size);
        song->size = params->size;
        return true;
    }

    const NuadeIoReadResult r = NuadeIo_read_url_to_memory(io, params->path);
    if (r.data == nullptr) {
        return false;
    }
    song->data = (u8*)malloc(r.size ? (size_t)r.size : 1);
    if (song->data != nullptr) {
        memcpy(song->data, r.data, (size_t)r.size);
        song->size = (size_t)r.size;
    }
    NuadeIo_free_url_to_memory(io, r.data);
    return song->data != nullptr;
}

NuadeSongResult nuade_open_ex(const NuadeOpenParams* params) {
    if (params == nullptr || (params->data == nullptr && params->path == nullptr)) {
        return fail(NuadeStatus_InvalidArgument, "neither data nor path given");
    }

    const NuadeIo* io = params->io ? params->io : nuade_io_default();
    NuadeSong* song = (NuadeSong*)calloc(1, sizeof(NuadeSong));
    if (song == nullptr) {
        return fail(NuadeStatus_OutOfMemory, "out of memory");
    }
    song->stereo_separation = 1.0f;

    if (params->path != nullptr && (song->path = nuade_strdup(params->path)) == nullptr) {
        free_song(song);
        return fail(NuadeStatus_OutOfMemory, "out of memory");
    }
    if (!load_data(song, params, io)) {
        free_song(song);
        return params->data ? fail(NuadeStatus_OutOfMemory, "out of memory")
                            : fail(NuadeStatus_IoError, "cannot read %s", params->path);
    }

    g_player = song->player = params->player ? params->player : probe_players(song->data, song->size, song->path); // rePlayer
    if (song->player == nullptr) {
        free_song(song);
        return fail(NuadeStatus_Unsupported, "no player recognises the song");
    }
    if ((song->player->tick == nullptr) == (song->player->render_raw == nullptr)) {
        const char* id = song->player->id;
        free_song(song);
        return fail(NuadeStatus_InvalidArgument, "player %s must implement exactly one of tick and render_raw", id);
    }

    song->io = io;
    const NuadeSongSource source = { .data = song->data, .size = song->size, .path = song->path, .io = io };
    const NuadePlayerOpenResult opened = song->player->open(&source);
    if (opened.user_data == nullptr) {
        NuadeSongResult result = { 0 };
        result.error = opened.error;
        if (result.error.status == NuadeStatus_Ok) {
            result.error.status = NuadeStatus_InvalidData;
        }
        free_song(song);
        return result;
    }
    song->user_data = opened.user_data;
    song->info = opened.info;
    song->own_display = song->info.display_voices != 0;
    if (!song->own_display) {
        song->info.display_voices = song->info.voice_count;
    }

    if (song->player->tick != nullptr) {
        if (song->info.timebase_hz != 0) {
            song->voices = nuade_mixer_create(song->info.voice_count);
        }
        if (song->voices == nullptr) {
            NuadeSongResult result = fail(song->info.timebase_hz ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData,
                                          "tick player %s: no timebase or out of memory", song->player->id);
            song->player->close(song->user_data);
            free_song(song);
            return result;
        }
    }

    song->viz = nuade_viz_create(song->info.voice_count, song->info.display_voices);
    if (song->viz == nullptr) {
        song->player->close(song->user_data);
        free_song(song);
        return fail(NuadeStatus_OutOfMemory, "out of memory");
    }

    if (song->player->metadata != nullptr) {
        const NuadeMetadataSink sink = { .ctx = song, .set_tag = sink_set_tag, .add_instrument = sink_add_instrument };
        song->player->metadata(song->user_data, &sink);
    }

    return (NuadeSongResult) { .song = song };
}

NuadeSongResult nuade_open(const char* path) {
    return nuade_open_ex(&(NuadeOpenParams) { .path = path });
}

NuadeSongResult nuade_open_memory(const void* data, size_t size, const char* path) {
    if (data == nullptr) {
        return fail(NuadeStatus_InvalidArgument, "no data");
    }
    return nuade_open_ex(&(NuadeOpenParams) { .path = path, .data = data, .size = size });
}

void nuade_close(NuadeSong* song) {
    if (song == nullptr) {
        return;
    }
    song->player->close(song->user_data);
    free_song(song);
}

const NuadeSongInfo* nuade_info(const NuadeSong* song) {
    return song ? &song->info : nullptr;
}

const NuadePlayer* nuade_player(const NuadeSong* song) {
    return song ? song->player : nullptr;
}

// rePlayer begin
void nuade_pan(NuadeSong* song, float* old_pan, float* new_pan) {
    nuade_mixer_pan(song->voices, old_pan, new_pan);
}
// rePlayer end

void* nuade_player_data(NuadeSong* song) {
    return song ? song->user_data : nullptr;
}

bool nuade_start(NuadeSong* song, uint32_t subsong, uint32_t sample_rate) {
    if (song == nullptr || subsong >= song->info.subsong_count || sample_rate == 0) {
        return false;
    }
    if (song->voices != nullptr) {
        nuade_mixer_reset(song->voices, sample_rate);
        nuade_scheduler_reset(&song->scheduler, song->info.timebase_hz, sample_rate);
        song->frames_until_tick = 0;
    }
    song->ticks_run = 0;
    song->ended = false;
    song->subsong = subsong;
    song->sample_rate = sample_rate;
    song->position = 0;
    song->loops = 0;
    nuade_viz_reset(song->viz);
    if (song->trace != nullptr) {
        nuade_trace_reset(song->trace, sample_rate);
    }
    return song->player->start(song->user_data, subsong, sample_rate, song->voices);
}

// A tick that returns no frames to mix is fine (several steps at one instant), but a player that never advances time
// would hang the render.
#define NUADE_MAX_EMPTY_TICKS 1024

// The clock the display follows for a player with a row grid and no position hook of its own: ticks per row, and how
// many ticks have run. Not running (ticks_per_row 0) for any other player.
static NuadeGridClock grid_clock(const NuadeSong* song) {
    const NuadePlayer* player = song->player;
    if (player->grid_ticks == nullptr || player->position != nullptr) {
        return (NuadeGridClock) { 0 };
    }
    return (NuadeGridClock) { .ticks_per_row = player->grid_ticks(song->user_data, song->subsong),
                              .ticks_run = song->ticks_run };
}

static NuadeRenderResult render_ticked(NuadeSong* song, float* stereo, float* const* voices, uint32_t frames) {
    if (stereo) // rePlayer
    memset(stereo, 0, (size_t)frames * 2 * sizeof(float));
    for (u32 v = 0; voices != nullptr && v < song->info.voice_count; v++) {
        memset(voices[v], 0, (size_t)frames * sizeof(float));
    }

    NuadeRenderResult result = { 0 };
    u32 empty_ticks = 0;
    while (result.frames < frames) {
        if (song->frames_until_tick == 0) {
            if (song->ended || empty_ticks == NUADE_MAX_EMPTY_TICKS) {
                result.flags |= NuadeRenderFlags_Ended;
                break;
            }
            const NuadeTickResult tick = song->player->tick(song->user_data, song->voices);
            song->ticks_run++;
            nuade_viz_record(song->viz, song->position + result.frames, song->player, song->user_data,
                             song->own_display ? nullptr : song->voices, grid_clock(song));
            if (tick.flags & NuadeTick_Looped) {
                song->ticks_run = 0; // the grid is one pass of the song
            }
            if (tick.flags & NuadeTick_Looped) {
                result.flags |= NuadeRenderFlags_Looped;
                // rePlayer begin
//                 if (++song->loops == song->repeat_count) {
//                     song->ended = true;
//                     result.flags |= NuadeRenderFlags_Ended;
//                     break;
//                 }
                break;
                // rePlayer end
            }
            if (tick.flags & NuadeTick_Ended) {
                song->ended = true;
                result.flags |= NuadeRenderFlags_Ended;
                break;
            }
            song->frames_until_tick = nuade_scheduler_frames(&song->scheduler, tick.length);
            empty_ticks = song->frames_until_tick == 0 ? empty_ticks + 1 : 0;
            continue;
        }

        const u32 count
            = song->frames_until_tick < frames - result.frames ? song->frames_until_tick : frames - result.frames;
        nuade_mixer_mix(song->voices, stereo, voices, result.frames, count);
        result.frames += count;
        song->frames_until_tick -= count;
    }
    return result;
}

// A raw player reports a loop only per call, so a call is cut into slices: when the slice that crosses the loop
// point ends the song, nothing past it is rendered, and every loop crossing is counted. The loop is therefore
// located to within one slice (a few ms): a raw player reports no crossing offset.
#define RAW_SLICE_FRAMES 256
#define RAW_MAX_VOICES 64

static NuadeRenderResult render_raw_sliced(NuadeSong* song, float* stereo, float* const* voices, uint32_t frames) {
    NuadeRenderResult total = { .frames = 0, .flags = NuadeRenderFlags_None };
    float* voice_slice[RAW_MAX_VOICES];
    const bool sliceable = voices == nullptr || song->info.voice_count <= RAW_MAX_VOICES;

    while (total.frames < frames && !song->ended) {
        const uint32_t left = frames - total.frames;
        const uint32_t count = sliceable && left > RAW_SLICE_FRAMES ? RAW_SLICE_FRAMES : left;
        float* const* slice_voices = nullptr;
        if (voices != nullptr && total.frames == 0) {
            slice_voices = voices;
        } else if (voices != nullptr) {
            for (uint32_t v = 0; v < song->info.voice_count; v++) {
                voice_slice[v] = voices[v] + total.frames;
            }
            slice_voices = voice_slice;
        }

        const NuadeRenderResult slice
            = song->player->render_raw(song->user_data, stereo + (size_t)total.frames * 2, slice_voices, count);
        total.frames += slice.frames;
        total.flags |= slice.flags;
        if ((slice.flags & NuadeRenderFlags_Looped) && ++song->loops == song->repeat_count) {
            song->ended = true;
            total.flags |= NuadeRenderFlags_Ended;
        }
        if ((slice.flags & NuadeRenderFlags_Ended) || slice.frames < count) {
            break;
        }
    }
    return total;
}

// Per-voice buffers for the meters when the caller wants no voice output of its own; 0 if memory ran out.
static float* const* meter_buffers(NuadeSong* song, u32 frames) {
    const u32 n = song->info.voice_count;
    if (n == 0) {
        return nullptr;
    }
    if (song->meter_voices == nullptr || song->meter_frames < frames) {
        free_meter_buffers(song);
        float** list = (float**)malloc(n * sizeof(float*));
        float* block = (float*)malloc((size_t)n * frames * sizeof(float));
        if (list == nullptr || block == nullptr) {
            free(list);
            free(block);
            return nullptr;
        }
        for (u32 v = 0; v < n; v++) {
            list[v] = block + (size_t)v * frames;
        }
        song->meter_voices = list;
        song->meter_frames = frames;
    }
    return song->meter_voices;
}

// Stereo separation for a self-rendering player, on its finished mix: keeps the mid (L + R) and scales the side
// (L - R). For voices panned hard left or right this is the same as scaling their pans, as the mixer does.
static void narrow_stereo(float* stereo, u32 frames, f32 separation) {
    if (separation == 1.0f) {
        return;
    }
    for (u32 i = 0; i < frames; i++) {
        const f32 mid = (stereo[i * 2] + stereo[i * 2 + 1]) * 0.5f;
        const f32 side = (stereo[i * 2] - stereo[i * 2 + 1]) * 0.5f * separation;
        stereo[i * 2] = mid + side;
        stereo[i * 2 + 1] = mid - side;
    }
}

NuadeRenderResult nuade_render(NuadeSong* song, float* stereo, float* const* voices, uint32_t frames) {
    if (song == nullptr || stereo == nullptr || song->ended) {
        return (NuadeRenderResult) { .frames = 0, .flags = NuadeRenderFlags_Ended };
    }
    float* const* metered = voices;
    if (metered == nullptr && nuade_viz_meters_on(song->viz)) {
        metered = meter_buffers(song, frames);
    }

    NuadeRenderResult result;
    if (song->voices != nullptr) {
        result = render_ticked(song, stereo, metered, frames);
    } else {
        result = render_raw_sliced(song, stereo, metered, frames);
        narrow_stereo(stereo, result.frames, song->stereo_separation);
        if (result.frames != 0) {
            // A self-rendering player is stamped once per call; its state is read after the call.
            nuade_viz_record(song->viz, song->position, song->player, song->user_data, nullptr, (NuadeGridClock) { 0 });
        }
    }
    if (metered != nullptr && nuade_viz_meters_on(song->viz)) {
        nuade_viz_feed(song->viz, metered, result.frames);
    }
    song->position += result.frames;
    return result;
}

NuadeSnapshotResult nuade_snapshot(const NuadeSong* song, uint64_t frame, NuadeVoiceView* voices, uint32_t cap) {
    if (song == nullptr) {
        return (NuadeSnapshotResult) { 0 };
    }
    return nuade_viz_find(song->viz, frame, voices, cap);
}

uint32_t nuade_events(NuadeSong* song, uint32_t voice, uint32_t track_pos, uint32_t before, uint32_t after,
                      NuadeEvent* out, uint32_t cap) {
    if (song == nullptr || out == nullptr || song->player->events == nullptr || voice >= song->info.display_voices) {
        return 0;
    }
    return song->player->events(song->user_data, voice, track_pos, before, after, out, cap);
}

// The scan of the current subsong for the row grid, made when none fits the subsong. nullptr if the
// player has no grid or the scan failed.
static const NuadeScan* grid_scan(NuadeSong* song) {
    if (song->player->grid_ticks == nullptr || song->player->tracker_info != nullptr
        || song->player->tracker_cells != nullptr || song->voices == nullptr) {
        return nullptr;
    }
    if (song->grid_scan != nullptr && song->grid_subsong != song->subsong) {
        nuade_scan_free(song->grid_scan);
        song->grid_scan = nullptr;
    }
    if (song->grid_scan == nullptr) {
        song->grid_scan = nuade_scan(song, song->subsong, GRID_SCAN_RATE, NUADE_GRID_MAX_TICKS);
        song->grid_subsong = song->subsong;
    }
    return song->grid_scan;
}

NuadeTrackerInfoResult nuade_tracker_info(NuadeSong* song, const NuadePosition* at) {
    if (song == nullptr || at == nullptr) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    if (song->player->tracker_info == nullptr) {
        const u32 ticks = song->player->grid_ticks != nullptr && at->order == 0
                              ? song->player->grid_ticks(song->user_data, song->subsong)
                              : 0;
        const NuadeScan* scan = ticks != 0 ? grid_scan(song) : nullptr;
        return at->order == 0 ? nuade_grid_info(scan, song->info.display_voices, ticks)
                              : (NuadeTrackerInfoResult) { 0 };
    }
    return song->player->tracker_info(song->user_data, at);
}

uint32_t nuade_tracker_cells(NuadeSong* song, const NuadePosition* at, uint32_t channel, int32_t row_lo, int32_t row_hi,
                             NuadePatternCell* out, uint32_t cap) {
    if (song == nullptr || at == nullptr || out == nullptr || channel >= song->info.display_voices) {
        return 0;
    }
    if (song->player->tracker_cells == nullptr) {
        const u32 ticks = song->player->grid_ticks != nullptr && at->order == 0
                              ? song->player->grid_ticks(song->user_data, song->subsong)
                              : 0;
        const NuadeScan* scan = ticks != 0 ? grid_scan(song) : nullptr;
        return nuade_grid_cells(scan, song->info.display_voices, ticks, at, channel, row_lo, row_hi, out, cap);
    }
    return song->player->tracker_cells(song->user_data, at, channel, row_lo, row_hi, out, cap);
}

uint32_t nuade_vu(NuadeSong* song, float* out, uint32_t cap) {
    if (song == nullptr || out == nullptr) {
        return 0;
    }
    nuade_viz_enable_meters(song->viz);
    return nuade_viz_meters_on(song->viz) ? nuade_viz_vu(song->viz, out, cap) : 0;
}

uint32_t nuade_scope(NuadeSong* song, uint32_t voice, float* out, uint32_t cap) {
    if (song == nullptr || out == nullptr) {
        return 0;
    }
    nuade_viz_enable_meters(song->viz);
    return nuade_viz_meters_on(song->viz) ? nuade_viz_scope(song->viz, voice, out, cap) : 0;
}

uint64_t nuade_position(const NuadeSong* song) {
    return song ? song->position : 0;
}

#define SKIP_FRAMES 4096

bool nuade_seek(NuadeSong* song, uint64_t frame) {
    if (song == nullptr || song->sample_rate == 0) {
        return false;
    }
    if (frame < song->position && !nuade_start(song, song->subsong, song->sample_rate)) {
        return false;
    }
    float scratch[SKIP_FRAMES * 2];
    while (song->position < frame) {
        const u64 left = frame - song->position;
        const NuadeRenderResult r = nuade_render(song, scratch, nullptr, left < SKIP_FRAMES ? (u32)left : SKIP_FRAMES);
        if ((r.flags & NuadeRenderFlags_Ended) || r.frames == 0) {
            break;
        }
    }
    nuade_viz_clear_meters(song->viz); // the skipped audio was never heard
    return song->position == frame;
}

bool nuade_duration(NuadeSong* song, uint32_t subsong, uint32_t sample_rate, uint64_t max_frames, NuadeDuration* out) {
    if (out == nullptr || !nuade_start(song, subsong, sample_rate)) {
        return false;
    }
    const u32 saved_repeat = song->repeat_count;
    song->repeat_count = 1; // the render then stops at the loop point itself

    float scratch[SKIP_FRAMES * 2];
    NuadeDuration d = { 0 };
    for (;;) {
        const u64 left = max_frames - d.frames;
        if (left == 0) {
            d.capped = true;
            break;
        }
        const NuadeRenderResult r = nuade_render(song, scratch, nullptr, left < SKIP_FRAMES ? (u32)left : SKIP_FRAMES);
        d.frames += r.frames;
        if (r.flags & NuadeRenderFlags_Looped) {
            d.looped = true;
            break;
        }
        if ((r.flags & NuadeRenderFlags_Ended) || r.frames == 0) {
            break;
        }
    }

    song->repeat_count = saved_repeat;
    *out = d;
    return nuade_start(song, subsong, sample_rate);
}

static void song_observer(const NuadeVoiceEvent* event, void* user) {
    NuadeSong* song = (NuadeSong*)user;
    nuade_trace_event(song->trace, event);
    if (song->observer != nullptr) {
        song->observer(event, song->observer_user);
    }
}

// The mixer calls song_observer while anything watches it.
static void update_mixer_observer(NuadeSong* song) {
    const bool watched = song->observer != nullptr || song->trace != nullptr;
    nuade_mixer_set_observer(song->voices, watched ? song_observer : nullptr, song);
}

void nuade_set_voice_observer(NuadeSong* song, NuadeVoiceObserver observer, void* user) {
    if (song != nullptr && song->voices != nullptr) {
        song->observer = observer;
        song->observer_user = user;
        update_mixer_observer(song);
    }
}

// Silent scan

typedef struct ScanCtx {
    NuadeScan* scan;
    u32 capacity;
    u32 tick;
    bool* idle;       // per voice: no sample is playing on it, so a queued one starts at once
    u32* note_starts; // own display voices: NuadeVoiceInfo.note_starts per display voice as last seen; else nullptr
    bool failed;
} ScanCtx;

static void scan_record(ScanCtx* ctx, u64 frame, u32 voice) {
    NuadeScan* scan = ctx->scan;
    if (scan->count == ctx->capacity) {
        const u32 capacity = ctx->capacity ? ctx->capacity * 2 : 256;
        NuadeScanEvent* events = (NuadeScanEvent*)realloc(scan->events, capacity * sizeof(NuadeScanEvent));
        if (events == nullptr) {
            ctx->failed = true;
            return;
        }
        scan->events = events;
        ctx->capacity = capacity;
    }
    scan->events[scan->count++]
        = (NuadeScanEvent) { .tick = ctx->tick, .frame = frame, .voice = voice, .note = -1, .instrument = -1 };
}

static void scan_observer(const NuadeVoiceEvent* event, void* user) {
    ScanCtx* ctx = (ScanCtx*)user;
    if (ctx->failed) {
        return;
    }
    bool* idle = &ctx->idle[event->voice];
    switch (event->kind) {
        case NuadeVoiceEvent_Play:
            scan_record(ctx, event->frame, event->voice);
            *idle = false;
            break;
        case NuadeVoiceEvent_Queue:
            if (*idle) {
                scan_record(ctx, event->frame, event->voice);
            }
            *idle = false;
            break;
        case NuadeVoiceEvent_Stop:
        case NuadeVoiceEvent_End:
            *idle = true;
            break;
        default:
            break;
    }
}

// After a tick: give the events recorded since `first` the note and instrument their voice reports now. With display
// voices of the player's own, record the notes it counted in the tick instead.
static void scan_annotate(const NuadeSong* song, NuadeVoiceInfo* info, ScanCtx* ctx, u32 first) {
    if (song->player->voice_info == nullptr || (ctx->note_starts == nullptr && first == ctx->scan->count)) {
        return;
    }
    for (u32 i = 0; i < song->info.display_voices; i++) {
        info[i] = (NuadeVoiceInfo) { .note = -1, .instrument = -1 };
    }
    song->player->voice_info(song->user_data, info, song->info.display_voices);
    for (u32 v = 0; ctx->note_starts != nullptr && v < song->info.display_voices; v++) {
        if (info[v].note_starts != ctx->note_starts[v]) {
            ctx->note_starts[v] = info[v].note_starts;
            scan_record(ctx, ctx->scan->frames, v);
        }
    }
    for (u32 i = first; i < ctx->scan->count; i++) {
        NuadeScanEvent* e = &ctx->scan->events[i];
        e->note = info[e->voice].note;
        e->instrument = info[e->voice].instrument;
    }
}

static void scan_run(NuadeSong* scratch, u32 max_ticks, ScanCtx* ctx, NuadeVoiceInfo* info) {
    NuadeScan* scan = ctx->scan;
    scan_annotate(scratch, info, ctx, 0);
    while (!ctx->failed) {
        if (scan->ticks == max_ticks) {
            scan->truncated = true;
            break;
        }
        const u32 first = scan->count;
        ctx->tick = scan->ticks;
        const NuadeTickResult tick = scratch->player->tick(scratch->user_data, scratch->voices);
        scan->ticks++;
        scan_annotate(scratch, info, ctx, first);
        if (tick.flags & (NuadeTick_Looped | NuadeTick_Ended)) {
            scan->looped = (tick.flags & NuadeTick_Looped) != 0;
            scan->ended = !scan->looped;
            break;
        }
        const u32 frames = nuade_scheduler_frames(&scratch->scheduler, tick.length);
        nuade_mixer_advance(scratch->voices, frames);
        scan->frames += frames;
    }
}

NuadeScan* nuade_scan(const NuadeSong* song, uint32_t subsong, uint32_t sample_rate, uint32_t max_ticks) {
    if (song == nullptr || song->voices == nullptr || subsong >= song->info.subsong_count || sample_rate == 0) {
        return nullptr;
    }
    const NuadeOpenParams params
        = { .path = song->path, .data = song->data, .size = song->size, .io = song->io, .player = song->player };
    const NuadeSongResult copy = nuade_open_ex(&params);
    NuadeScan* scan = (NuadeScan*)calloc(1, sizeof(NuadeScan));
    NuadeVoiceInfo* info = (NuadeVoiceInfo*)calloc(song->info.display_voices, sizeof(NuadeVoiceInfo));
    ScanCtx ctx = { .scan = scan, .idle = (bool*)malloc(song->info.voice_count * sizeof(bool)) };
    if (song->own_display) {
        ctx.note_starts = (u32*)calloc(song->info.display_voices, sizeof(u32));
    }

    bool ok = copy.song != nullptr && scan != nullptr && info != nullptr && ctx.idle != nullptr
              && (ctx.note_starts != nullptr || !song->own_display);
    if (ok) {
        for (u32 i = 0; i < song->info.voice_count; i++) {
            ctx.idle[i] = true;
        }
        if (!song->own_display) {
            nuade_set_voice_observer(copy.song, scan_observer, &ctx);
        }
        ok = nuade_start(copy.song, subsong, sample_rate);
    }
    if (ok) {
        scan_run(copy.song, max_ticks, &ctx, info);
    }
    nuade_close(copy.song);
    free(ctx.idle);
    free(ctx.note_starts);
    free(info);
    if (!ok || ctx.failed) {
        nuade_scan_free(scan);
        return nullptr;
    }
    return scan;
}

void nuade_scan_free(NuadeScan* scan) {
    if (scan != nullptr) {
        free(scan->events);
        free(scan);
    }
}

bool nuade_set_paula_trace(NuadeSong* song, const char* path, bool with_bytes) {
    if (song == nullptr || song->voices == nullptr) {
        return false;
    }
    NuadeTrace* trace = nullptr;
    if (path != nullptr && path[0] != '\0') {
        FILE* file = fopen(path, "w");
        trace = file ? nuade_trace_create(file, song->info.voice_count, with_bytes) : nullptr;
        if (trace == nullptr) {
            if (file != nullptr) {
                fclose(file);
            }
            return false;
        }
        nuade_trace_reset(trace, song->sample_rate);
    }
    nuade_trace_destroy(song->trace);
    song->trace = trace;
    update_mixer_observer(song);
    return true;
}

bool nuade_set_dump_state(NuadeSong* song, const char* path) {
    if (song == nullptr || path == nullptr || song->player->dump_state == nullptr) {
        return false;
    }
    return song->player->dump_state(song->user_data, path);
}

void nuade_set_repeat_count(NuadeSong* song, uint32_t count) {
    if (song != nullptr) {
        song->repeat_count = count;
    }
}

bool nuade_set_stereo_separation(NuadeSong* song, float separation) {
    if (song == nullptr || !(separation >= 0.0f && separation <= 1.0f)) {
        return false;
    }
    song->stereo_separation = separation;
    if (song->voices != nullptr) {
        nuade_mixer_set_separation(song->voices, separation);
    }
    return true;
}

const char* nuade_meta(const NuadeSong* song, const char* key) {
    if (song == nullptr || key == nullptr) {
        return nullptr;
    }
    for (u32 i = 0; i < song->tag_count; i++) {
        if (strcmp(song->tags[i].key, key) == 0) {
            return song->tags[i].value;
        }
    }
    return nullptr;
}

uint32_t nuade_instrument_count(const NuadeSong* song) {
    return song ? song->instrument_count : 0;
}

const char* nuade_instrument_name(const NuadeSong* song, uint32_t n, uint32_t* index) {
    if (song == nullptr || n >= song->instrument_count) {
        return nullptr;
    }
    if (index != nullptr) {
        *index = song->instruments[n].index;
    }
    return song->instruments[n].name;
}
