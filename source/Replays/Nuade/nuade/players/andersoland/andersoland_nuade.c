// SPDX-License-Identifier: MIT
// Anders 0land as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/andersoland.h>
#include <stdio.h>

static NuadeProbe andersoland_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ao_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult andersoland_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    AoPlayer* player = ao_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Anders 0land module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = AO_VOICES,
                                    .subsong_count = (uint32_t)ao_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = AO_TIMEBASE_HZ };
    return result;
}

static void andersoland_close(void* user_data) {
    ao_destroy((AoPlayer*)user_data);
}

static bool andersoland_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return ao_start((AoPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult andersoland_tick(void* user_data, NuadeVoices* voices) {
    const AoTickResult tick = ao_tick((AoPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = AO_TICK_LENGTH, .flags = flags };
}

static void andersoland_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Anders 0land");
}

static uint32_t andersoland_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const AoPlayer* player = (const AoPlayer*)user_data;
    const uint32_t n = cap < AO_VOICES ? cap : AO_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const AoVoiceState v = ao_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t andersoland_grid_ticks(void* user_data, uint32_t subsong) {
    return ao_grid_ticks((const AoPlayer*)user_data, (int)subsong);
}

const NuadePlayer nuade_player_andersoland = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "andersoland",
    .name = "Anders 0land",
    .version = "1",
    .extensions = "hot",
    .probe = andersoland_probe,
    .open = andersoland_open,
    .close = andersoland_close,
    .start = andersoland_start,
    .tick = andersoland_tick,
    .metadata = andersoland_metadata,
    .voice_info = andersoland_voice_info,
    .grid_ticks = andersoland_grid_ticks,
};

AoPlayer* ao_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_andersoland) {
        return nullptr;
    }
    return (AoPlayer*)nuade_player_data(song);
}
