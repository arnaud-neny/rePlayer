// SPDX-License-Identifier: MIT
// Jesper Olsen player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/jesperolsen.h>
#include <stdio.h>

// Ticks one row of the display grid spans: the voices run their own note lengths, so the grid is a display choice only
// (4: no two starts of one voice share a row in the test songs).
#define GRID_TICKS 4

static NuadeProbe jesperolsen_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return jo_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult jesperolsen_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    JoPlayer* player = jo_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Jesper Olsen module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = JO_VOICES,
                                    .subsong_count = (uint32_t)jo_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = JO_TIMEBASE_HZ };
    return result;
}

static void jesperolsen_close(void* user_data) {
    jo_destroy((JoPlayer*)user_data);
}

static bool jesperolsen_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    (void)voices;
    return jo_start((JoPlayer*)user_data, (int)subsong);
}

static NuadeTickResult jesperolsen_tick(void* user_data, NuadeVoices* voices) {
    const JoTickResult tick = jo_tick((JoPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = JO_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : NuadeTick_None) };
}

static void jesperolsen_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jesper Olsen");
    sink->set_tag(sink->ctx, NUADE_META_TOOL, jo_replayer_name((const JoPlayer*)user_data));
}

static uint32_t jesperolsen_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const JoPlayer* player = (const JoPlayer*)user_data;
    const uint32_t n = cap < JO_VOICES ? cap : JO_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const JoVoiceState v = jo_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t jesperolsen_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_jesperolsen = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "jesperolsen",
    .name = "Jesper Olsen",
    .version = "1",
    .extensions = "jo",
    .probe = jesperolsen_probe,
    .open = jesperolsen_open,
    .close = jesperolsen_close,
    .start = jesperolsen_start,
    .tick = jesperolsen_tick,
    .metadata = jesperolsen_metadata,
    .voice_info = jesperolsen_voice_info,
    .grid_ticks = jesperolsen_grid_ticks,
};

JoPlayer* jo_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_jesperolsen) {
        return nullptr;
    }
    return (JoPlayer*)nuade_player_data(song);
}
