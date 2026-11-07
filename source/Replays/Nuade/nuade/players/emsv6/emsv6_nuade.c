// SPDX-License-Identifier: MIT
// EMS v6 (Electronic Music System v6) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/emsv6.h>
#include <stdio.h>

// Ticks one row of the display grid spans: the tracks run their own row lengths, so the grid is a display choice only
// (4: no two starts of one track share a row in the test songs).
#define GRID_TICKS 4

static NuadeProbe emsv6_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ems6_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult emsv6_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Ems6Player* player = ems6_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid EMS v6 module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = EMS6_VOICES,
                                    .subsong_count = 1,
                                    .default_subsong = 0,
                                    .timebase_hz = EMS6_TIMEBASE_HZ,
                                    .display_voices = ems6_track_count(player) };
    return result;
}

static void emsv6_close(void* user_data) {
    ems6_destroy((Ems6Player*)user_data);
}

static bool emsv6_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ems6_start((Ems6Player*)user_data, voices);
}

static NuadeTickResult emsv6_tick(void* user_data, NuadeVoices* voices) {
    const Ems6TickResult tick = ems6_tick((Ems6Player*)user_data, voices);
    return (NuadeTickResult) { .length = EMS6_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void emsv6_metadata(void* user_data, const NuadeMetadataSink* sink) {
    char format[64];
    snprintf(format, sizeof(format), "EMS v%s (Electronic Music System)", ems6_version((const Ems6Player*)user_data));
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, format);
}

// The display voices are the tracks the song uses; the Paula voices play the mix.
static uint32_t emsv6_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const Ems6Player* player = (const Ems6Player*)user_data;
    const uint32_t tracks = ems6_track_count(player);
    const uint32_t n = cap < tracks ? cap : tracks;
    for (uint32_t i = 0; i < n; i++) {
        const Ems6TrackState t = ems6_track_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = t.note, .instrument = t.instrument, .note_starts = t.note_starts };
    }
    return n;
}

static uint32_t emsv6_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_emsv6 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "emsv6",
    .name = "EMS v6 (Electronic Music System)",
    .version = "1",
    .extensions = "ems",
    .probe = emsv6_probe,
    .open = emsv6_open,
    .close = emsv6_close,
    .start = emsv6_start,
    .tick = emsv6_tick,
    .metadata = emsv6_metadata,
    .voice_info = emsv6_voice_info,
    .grid_ticks = emsv6_grid_ticks,
};

Ems6Player* ems6_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_emsv6) {
        return nullptr;
    }
    return (Ems6Player*)nuade_player_data(song);
}
