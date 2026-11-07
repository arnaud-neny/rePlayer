// SPDX-License-Identifier: MIT
// EarAche as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/earache.h>
#include <stdio.h>

static NuadeProbe earache_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ea_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult earache_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    EaPlayer* player = ea_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not an EarAche song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = EA_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = EA_TIMEBASE_HZ
    };
    return result;
}

static void earache_close(void* user_data) {
    ea_destroy((EaPlayer*)user_data);
}

static bool earache_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return ea_start((EaPlayer*)user_data, voices);
}

static NuadeTickResult earache_tick(void* user_data, NuadeVoices* voices) {
    const EaTickResult tick = ea_tick((EaPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = EA_TICK_LENGTH, .flags = flags };
}

static void earache_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "EarAche");
}

static uint32_t earache_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const EaPlayer* player = (const EaPlayer*)user_data;
    const uint32_t n = cap < EA_VOICES ? cap : EA_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const EaVoiceState v = ea_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t earache_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return ea_grid_ticks((const EaPlayer*)user_data);
}

const NuadePlayer nuade_player_earache = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "earache",
    .name = "EarAche",
    .version = "1",
    .extensions = "ea,mg",
    .probe = earache_probe,
    .open = earache_open,
    .close = earache_close,
    .start = earache_start,
    .tick = earache_tick,
    .metadata = earache_metadata,
    .voice_info = earache_voice_info,
    .grid_ticks = earache_grid_ticks,
};

EaPlayer* ea_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_earache) {
        return nullptr;
    }
    return (EaPlayer*)nuade_player_data(song);
}
