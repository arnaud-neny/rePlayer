// SPDX-License-Identifier: MIT
// Speedy System as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/speedysystem.h>
#include <stdio.h>

static NuadeProbe speedysystem_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ss_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult speedysystem_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SsPlayer* player = ss_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Speedy System song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = SS_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SS_TIMEBASE_HZ
    };
    return result;
}

static void speedysystem_close(void* user_data) {
    ss_destroy((SsPlayer*)user_data);
}

static bool speedysystem_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ss_start((SsPlayer*)user_data, voices);
}

static NuadeTickResult speedysystem_tick(void* user_data, NuadeVoices* voices) {
    const SsTickResult tick = ss_tick((SsPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SS_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void speedysystem_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Speedy System");
}

static uint32_t speedysystem_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SsPlayer* player = (const SsPlayer*)user_data;
    const uint32_t n = cap < SS_VOICES ? cap : SS_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SsVoiceState v = ss_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t speedysystem_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return ss_grid_ticks((const SsPlayer*)user_data);
}

const NuadePlayer nuade_player_speedysystem = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "speedysystem",
    .name = "Speedy System",
    .version = "1",
    .extensions = "ss",
    .probe = speedysystem_probe,
    .open = speedysystem_open,
    .close = speedysystem_close,
    .start = speedysystem_start,
    .tick = speedysystem_tick,
    .metadata = speedysystem_metadata,
    .voice_info = speedysystem_voice_info,
    .grid_ticks = speedysystem_grid_ticks,
};

SsPlayer* ss_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_speedysystem) {
        return nullptr;
    }
    return (SsPlayer*)nuade_player_data(song);
}
