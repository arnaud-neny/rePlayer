// SPDX-License-Identifier: MIT
// Speedy A1 System as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/speedya1system.h>
#include <stdio.h>

static NuadeProbe speedya1system_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sa1_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult speedya1system_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Sa1Player* player = sa1_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Speedy A1 System song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = SA1_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SA1_TIMEBASE_HZ
    };
    return result;
}

static void speedya1system_close(void* user_data) {
    sa1_destroy((Sa1Player*)user_data);
}

static bool speedya1system_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && sa1_start((Sa1Player*)user_data, voices);
}

static NuadeTickResult speedya1system_tick(void* user_data, NuadeVoices* voices) {
    const Sa1TickResult tick = sa1_tick((Sa1Player*)user_data, voices);
    return (NuadeTickResult) { .length = SA1_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void speedya1system_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Speedy A1 System");
}

static uint32_t speedya1system_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const Sa1Player* player = (const Sa1Player*)user_data;
    const uint32_t n = cap < SA1_VOICES ? cap : SA1_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Sa1VoiceState v = sa1_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t speedya1system_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return sa1_grid_ticks((const Sa1Player*)user_data);
}

const NuadePlayer nuade_player_speedya1system = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "speedya1system",
    .name = "Speedy A1 System",
    .version = "1",
    .extensions = "sas",
    .probe = speedya1system_probe,
    .open = speedya1system_open,
    .close = speedya1system_close,
    .start = speedya1system_start,
    .tick = speedya1system_tick,
    .metadata = speedya1system_metadata,
    .voice_info = speedya1system_voice_info,
    .grid_ticks = speedya1system_grid_ticks,
};

Sa1Player* sa1_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_speedya1system) {
        return nullptr;
    }
    return (Sa1Player*)nuade_player_data(song);
}
