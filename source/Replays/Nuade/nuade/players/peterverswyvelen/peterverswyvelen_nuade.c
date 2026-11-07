// SPDX-License-Identifier: MIT
// Peter Verswyvelen as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/peterverswyvelen.h>
#include <stdio.h>

static NuadeProbe peterverswyvelen_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return pv_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult peterverswyvelen_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    PvPlayer* player = pv_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Peter Verswyvelen song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = PV_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = PV_TIMEBASE_HZ
    };
    return result;
}

static void peterverswyvelen_close(void* user_data) {
    pv_destroy((PvPlayer*)user_data);
}

static bool peterverswyvelen_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && pv_start((PvPlayer*)user_data, voices);
}

static NuadeTickResult peterverswyvelen_tick(void* user_data, NuadeVoices* voices) {
    const PvTickResult tick = pv_tick((PvPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = PV_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void peterverswyvelen_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Peter Verswyvelen");
}

static uint32_t peterverswyvelen_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PvPlayer* player = (const PvPlayer*)user_data;
    const uint32_t n = cap < PV_VOICES ? cap : PV_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const PvVoiceState v = pv_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t peterverswyvelen_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return pv_grid_ticks((const PvPlayer*)user_data);
}

const NuadePlayer nuade_player_peterverswyvelen = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "peterverswyvelen",
    .name = "Peter Verswyvelen",
    .version = "1",
    .extensions = "pvp",
    .probe = peterverswyvelen_probe,
    .open = peterverswyvelen_open,
    .close = peterverswyvelen_close,
    .start = peterverswyvelen_start,
    .tick = peterverswyvelen_tick,
    .metadata = peterverswyvelen_metadata,
    .voice_info = peterverswyvelen_voice_info,
    .grid_ticks = peterverswyvelen_grid_ticks,
};

PvPlayer* pv_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_peterverswyvelen) {
        return nullptr;
    }
    return (PvPlayer*)nuade_player_data(song);
}
