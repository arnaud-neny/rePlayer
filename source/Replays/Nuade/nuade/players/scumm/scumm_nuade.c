// SPDX-License-Identifier: MIT
// SCUMM sound drivers as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/scumm.h>
#include <stdio.h>

#define SCUMM_GRID_TICKS 6 // the voices run on their own durations: a display grid only

static NuadeProbe scumm_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return scumm_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult scumm_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ScummPlayer* player = scumm_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known SCUMM sound driver");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = SCUMM_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SCUMM_TIMEBASE_HZ
    };
    return result;
}

static void scumm_nuade_close(void* user_data) {
    scumm_destroy((ScummPlayer*)user_data);
}

static bool scumm_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    (void)voices;
    return scumm_start((ScummPlayer*)user_data);
}

static NuadeTickResult scumm_nuade_tick(void* user_data, NuadeVoices* voices) {
    scumm_tick((ScummPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SCUMM_TICK_LENGTH, .flags = NuadeTick_None };
}

static void scumm_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "SCUMM");
}

static uint32_t scumm_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ScummPlayer* player = (const ScummPlayer*)user_data;
    const uint32_t n = cap < SCUMM_VOICES ? cap : SCUMM_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const ScummVoiceState v = scumm_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t scumm_nuade_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return SCUMM_GRID_TICKS;
}

const NuadePlayer nuade_player_scumm = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "scumm",
    .name = "SCUMM",
    .version = "1",
    .extensions = "scumm",
    .probe = scumm_nuade_probe,
    .open = scumm_nuade_open,
    .close = scumm_nuade_close,
    .start = scumm_nuade_start,
    .tick = scumm_nuade_tick,
    .metadata = scumm_nuade_metadata,
    .voice_info = scumm_nuade_voice_info,
    .grid_ticks = scumm_nuade_grid_ticks,
};

ScummPlayer* scumm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_scumm) {
        return nullptr;
    }
    return (ScummPlayer*)nuade_player_data(song);
}
