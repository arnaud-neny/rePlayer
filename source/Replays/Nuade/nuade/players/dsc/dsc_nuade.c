// SPDX-License-Identifier: MIT
// Digital Sonix & Chrome as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/dsc.h>
#include <stdio.h>

static NuadeProbe dsc_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dsc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult dsc_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DscPlayer* player = dsc_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Digital Sonix & Chrome module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = DSC_VOICES,
                                    .subsong_count = (uint32_t)dsc_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = DSC_TIMEBASE_HZ };
    return result;
}

static void dsc_nuade_close(void* user_data) {
    dsc_destroy((DscPlayer*)user_data);
}

static bool dsc_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dsc_start((DscPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult dsc_nuade_tick(void* user_data, NuadeVoices* voices) {
    const DscTickResult tick = dsc_tick((DscPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = DSC_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void dsc_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Digital Sonix & Chrome");
}

static uint32_t dsc_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DscPlayer* player = (const DscPlayer*)user_data;
    const uint32_t n = cap < DSC_VOICES ? cap : DSC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = (NuadeVoiceInfo) { .note = -1, .instrument = dsc_voice_state(player, (int)i).sound };
    }
    return n;
}

// One grid row is one song line.
static uint32_t dsc_nuade_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)dsc_tempo((const DscPlayer*)user_data);
}

const NuadePlayer nuade_player_dsc = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "dsc",
    .name = "Digital Sonix & Chrome",
    .version = "1",
    .extensions = "dsc",
    .probe = dsc_nuade_probe,
    .open = dsc_nuade_open,
    .close = dsc_nuade_close,
    .start = dsc_nuade_start,
    .tick = dsc_nuade_tick,
    .metadata = dsc_nuade_metadata,
    .voice_info = dsc_nuade_voice_info,
    .grid_ticks = dsc_nuade_grid_ticks,
};

DscPlayer* dsc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_dsc) {
        return nullptr;
    }
    return (DscPlayer*)nuade_player_data(song);
}
