// SPDX-License-Identifier: MIT
// David Whittaker as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/davidwhittaker.h>
#include <stdio.h>

// The identification scans for the player's 68k code: specific, but not a mark.
static NuadeProbe davidwhittaker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dw_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult davidwhittaker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DwModule* module = dw_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid David Whittaker module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = (uint32_t)dw_voice_count(module),
        .subsong_count = (uint32_t)dw_subsong_count(module),
        .default_subsong = 0,
        .timebase_hz = DW_TICK_HZ,
    };
    return result;
}

static void davidwhittaker_close(void* user_data) {
    dw_destroy((DwModule*)user_data);
}

static bool davidwhittaker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dw_start((DwModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult davidwhittaker_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    DwModule* module = (DwModule*)user_data;
    const uint32_t loops_before = dw_loop_count(module);
    dw_tick(module);

    uint32_t flags = NuadeTick_None;
    if (dw_loop_count(module) != loops_before) {
        flags |= NuadeTick_Looped;
    }
    if (dw_ended(module)) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = 1, .flags = flags };
}

static void davidwhittaker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "David Whittaker");
}

// A note lasts a multiple of the song speed in ticks, so one row of the grid is one speed step.
static uint32_t davidwhittaker_grid_ticks(void* user_data, uint32_t subsong) {
    return dw_subsong_speed((DwModule*)user_data, (int)subsong);
}

static uint32_t davidwhittaker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DwModule* module = (const DwModule*)user_data;
    const uint32_t count = (uint32_t)dw_voice_count(module);
    const uint32_t n = count < cap ? count : cap;
    for (uint32_t i = 0; i < n; i++) {
        int note = -1;
        int instrument = -1;
        dw_voice_started(module, (int)i, &note, &instrument);
        out[i] = (NuadeVoiceInfo) { .note = note, .instrument = instrument };
    }
    return n;
}

const NuadePlayer nuade_player_davidwhittaker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "davidwhittaker",
    .name = "David Whittaker",
    .version = "1",
    .extensions = "dw,dwold",
    .probe = davidwhittaker_probe,
    .open = davidwhittaker_open,
    .close = davidwhittaker_close,
    .start = davidwhittaker_start,
    .tick = davidwhittaker_tick,
    .metadata = davidwhittaker_metadata,
    .voice_info = davidwhittaker_voice_info,
    .grid_ticks = davidwhittaker_grid_ticks,
};

DwModule* dw_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_davidwhittaker) {
        return nullptr;
    }
    return (DwModule*)nuade_player_data(song);
}
