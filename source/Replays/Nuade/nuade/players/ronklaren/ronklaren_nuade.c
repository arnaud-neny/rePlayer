// SPDX-License-Identifier: MIT
// Ron Klaren sound modules as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/ronklaren.h>
#include <stdio.h>

// The hunk header followed by the module mark is specific enough to be sure.
static NuadeProbe ronklaren_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return rk_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult ronklaren_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    RkModule* module = rk_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Ron Klaren module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = (uint32_t)rk_channel_count(module),
                                    .subsong_count = (uint32_t)rk_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = RK_TIMEBASE_HZ };
    return result;
}

static void ronklaren_close(void* user_data) {
    rk_destroy((RkModule*)user_data);
}

static bool ronklaren_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return rk_start((RkModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult ronklaren_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    RkModule* module = (RkModule*)user_data;
    return (NuadeTickResult) { .length = rk_tick_length(module),
                               .flags = rk_tick(module) ? NuadeTick_Looped : NuadeTick_None };
}

static void ronklaren_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Ron Klaren");
}

static uint32_t ronklaren_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const RkModule* module = (const RkModule*)user_data;
    const uint32_t n = cap < RK_CHANNELS ? cap : RK_CHANNELS;
    for (uint32_t i = 0; i < n; i++) {
        const RkVoiceState v = rk_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t ronklaren_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return RK_TICKS_PER_ROW;
}

const NuadePlayer nuade_player_ronklaren = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "ronklaren",
    .name = "Ron Klaren",
    .version = "1",
    .extensions = "rk,rkb",
    .probe = ronklaren_probe,
    .open = ronklaren_open,
    .close = ronklaren_close,
    .start = ronklaren_start,
    .tick = ronklaren_tick,
    .metadata = ronklaren_metadata,
    .voice_info = ronklaren_voice_info,
    .grid_ticks = ronklaren_grid_ticks,
};

RkModule* rk_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_ronklaren) {
        return nullptr;
    }
    return (RkModule*)nuade_player_data(song);
}
