// SPDX-License-Identifier: MIT
// Andrew Parton as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/andrewparton.h>
#include <stdio.h>

// The grid of the pattern view: the events come at any tick.
#define GRID_TICKS 3

static NuadeProbe andrewparton_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ap_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult andrewparton_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ApPlayer* player = ap_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Andrew Parton module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = AP_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = AP_TIMEBASE_HZ
    };
    return result;
}

static void andrewparton_close(void* user_data) {
    ap_destroy((ApPlayer*)user_data);
}

static bool andrewparton_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ap_start((ApPlayer*)user_data, voices);
}

static NuadeTickResult andrewparton_tick(void* user_data, NuadeVoices* voices) {
    const ApTickResult tick = ap_tick((ApPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = AP_TICK_LENGTH, .flags = tick.ended ? NuadeTick_Ended : NuadeTick_None };
}

static void andrewparton_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Andrew Parton");
}

static uint32_t andrewparton_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ApPlayer* player = (const ApPlayer*)user_data;
    const uint32_t n = cap < AP_VOICES ? cap : AP_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const ApVoiceState v = ap_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t andrewparton_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_andrewparton = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "andrewparton",
    .name = "Andrew Parton",
    .version = "1",
    .extensions = "bye",
    .probe = andrewparton_probe,
    .open = andrewparton_open,
    .close = andrewparton_close,
    .start = andrewparton_start,
    .tick = andrewparton_tick,
    .metadata = andrewparton_metadata,
    .voice_info = andrewparton_voice_info,
    .grid_ticks = andrewparton_grid_ticks,
};

ApPlayer* ap_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_andrewparton) {
        return nullptr;
    }
    return (ApPlayer*)nuade_player_data(song);
}
