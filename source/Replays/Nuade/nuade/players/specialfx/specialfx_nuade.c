// SPDX-License-Identifier: MIT
// Special FX sound driver (.jd) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/specialfx.h>
#include <stdio.h>

// Ticks one row of the display grid spans: the driver's voices run their own note lengths, so the grid is a display
// choice only.
#define JD_GRID_TICKS 6

static NuadeProbe specialfx_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return jd_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult specialfx_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    JdPlayer* player = jd_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Special FX module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = (uint32_t)jd_voice_count(player),
                                    .subsong_count = (uint32_t)jd_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = JD_TIMEBASE_HZ };
    return result;
}

static void specialfx_close(void* user_data) {
    jd_destroy((JdPlayer*)user_data);
}

static bool specialfx_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return jd_start((JdPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult specialfx_tick(void* user_data, NuadeVoices* voices) {
    const JdTickResult tick = jd_tick((JdPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = JD_TICK_LENGTH, .flags = flags };
}

static void specialfx_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Special FX");
}

static uint32_t specialfx_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const JdPlayer* player = (const JdPlayer*)user_data;
    const uint32_t voices = (uint32_t)jd_voice_count(player);
    const uint32_t count = cap < voices ? cap : voices;
    for (uint32_t i = 0; i < count; i++) {
        const JdVoiceState state = jd_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = state.note, .instrument = state.instrument };
    }
    return count;
}

static uint32_t specialfx_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return JD_GRID_TICKS;
}

const NuadePlayer nuade_player_specialfx = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "specialfx",
    .name = "Special FX",
    .version = "1",
    .extensions = "jd",
    .probe = specialfx_probe,
    .open = specialfx_open,
    .close = specialfx_close,
    .start = specialfx_start,
    .tick = specialfx_tick,
    .metadata = specialfx_metadata,
    .voice_info = specialfx_voice_info,
    .grid_ticks = specialfx_grid_ticks,
};

JdPlayer* jd_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_specialfx) {
        return nullptr;
    }
    return (JdPlayer*)nuade_player_data(song);
}
