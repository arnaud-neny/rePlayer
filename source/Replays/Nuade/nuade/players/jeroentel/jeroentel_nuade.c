// SPDX-License-Identifier: MIT
// Jeroen Tel player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/jeroentel.h>
#include <stdio.h>

static NuadeProbe jeroentel_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return jt_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult jeroentel_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    JtPlayer* player = jt_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Jeroen Tel module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = JT_VOICES,
                                    .subsong_count = (uint32_t)jt_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = JT_TIMEBASE_HZ };
    return result;
}

static void jeroentel_close(void* user_data) {
    jt_destroy((JtPlayer*)user_data);
}

static bool jeroentel_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return jt_start((JtPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult jeroentel_tick(void* user_data, NuadeVoices* voices) {
    const JtTickResult tick = jt_tick((JtPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = JT_TICK_LENGTH, .flags = flags };
}

static void jeroentel_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jeroen Tel");
}

static uint32_t jeroentel_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const JtPlayer* player = (const JtPlayer*)user_data;
    const uint32_t count = cap < JT_VOICES ? cap : JT_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const JtVoiceState v = jt_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t jeroentel_grid_ticks(void* user_data, uint32_t subsong) {
    return (uint32_t)jt_ticks_per_step((const JtPlayer*)user_data, (int)subsong);
}

const NuadePlayer nuade_player_jeroentel = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "jeroentel",
    .name = "Jeroen Tel",
    .version = "1",
    .extensions = "jt,mon_old",
    .probe = jeroentel_probe,
    .open = jeroentel_open,
    .close = jeroentel_close,
    .start = jeroentel_start,
    .tick = jeroentel_tick,
    .metadata = jeroentel_metadata,
    .voice_info = jeroentel_voice_info,
    .grid_ticks = jeroentel_grid_ticks,
};

JtPlayer* jt_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_jeroentel) {
        return nullptr;
    }
    return (JtPlayer*)nuade_player_data(song);
}
