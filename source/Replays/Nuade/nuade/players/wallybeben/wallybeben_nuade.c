// SPDX-License-Identifier: MIT
// Wally Beben as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/wallybeben.h>

#include <stdio.h>

#define WB_GRID_TICKS 6 // the voices run on their own durations: a display grid only

static NuadeProbe wallybeben_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return wb_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult wallybeben_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    WbPlayer* player = wb_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Wally Beben song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = WB_VOICES,
                                    .subsong_count = wb_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = WB_TIMEBASE_HZ };
    return result;
}

static void wallybeben_close(void* user_data) {
    wb_destroy((WbPlayer*)user_data);
}

static bool wallybeben_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return wb_start((WbPlayer*)user_data, subsong, voices);
}

static NuadeTickResult wallybeben_tick(void* user_data, NuadeVoices* voices) {
    const WbTickResult tick = wb_tick((WbPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    flags |= tick.looped ? NuadeTick_Looped : 0u;
    flags |= tick.ended ? NuadeTick_Ended : 0u;
    return (NuadeTickResult) { .length = WB_TICK_LENGTH, .flags = flags };
}

static void wallybeben_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const WbPlayer* player = (const WbPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Wally Beben");
    sink->set_tag(sink->ctx, NUADE_META_ARTIST, "Wally Beben");
    sink->set_tag(sink->ctx, NUADE_META_TOOL, wb_version_name(player));
}

static uint32_t wallybeben_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const WbPlayer* player = (const WbPlayer*)user_data;
    const uint32_t n = cap < WB_VOICES ? cap : WB_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const WbVoiceState v = wb_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t wallybeben_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return WB_GRID_TICKS;
}

const NuadePlayer nuade_player_wallybeben = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "wallybeben",
    .name = "Wally Beben",
    .version = "1",
    .extensions = "wb",
    .probe = wallybeben_probe,
    .open = wallybeben_open,
    .close = wallybeben_close,
    .start = wallybeben_start,
    .tick = wallybeben_tick,
    .metadata = wallybeben_metadata,
    .voice_info = wallybeben_voice_info,
    .grid_ticks = wallybeben_grid_ticks,
};

WbPlayer* wb_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_wallybeben) {
        return nullptr;
    }
    return (WbPlayer*)nuade_player_data(song);
}
