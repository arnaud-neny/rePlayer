// SPDX-License-Identifier: MIT
// Kim Christensen as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_path.h"
#include "nuade_tracker.h"
#include <nuade/players/kimchristensen.h>
#include <stdio.h>

// The eagleplayer finds its songs by the "kim" prefix or suffix only: the data check is too loose to claim a file.

static NuadeProbe kimchristensen_probe(const uint8_t* data, size_t size, const char* filename) {
    if (filename == nullptr || !nuade_path_has_ext(filename, "kim")) {
        return NuadeProbe_Unsupported;
    }
    KimPlayer* player = kim_create(data, size);
    if (player == nullptr) {
        return NuadeProbe_Unsupported;
    }
    kim_destroy(player);
    return NuadeProbe_Supported;
}

static NuadePlayerOpenResult kimchristensen_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    KimPlayer* player = kim_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Kim Christensen song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = KIM_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = KIM_TIMEBASE_HZ
    };
    return result;
}

static void kimchristensen_close(void* user_data) {
    kim_destroy((KimPlayer*)user_data);
}

static bool kimchristensen_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return kim_start((KimPlayer*)user_data, voices);
}

static NuadeTickResult kimchristensen_tick(void* user_data, NuadeVoices* voices) {
    const KimTickResult tick = kim_tick((KimPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = KIM_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void kimchristensen_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Kim Christensen");
}

static uint32_t kimchristensen_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const KimPlayer* player = (const KimPlayer*)user_data;
    const uint32_t n = cap < KIM_VOICES ? cap : KIM_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const KimVoiceState v = kim_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t kimchristensen_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return kim_grid_ticks((const KimPlayer*)user_data);
}

const NuadePlayer nuade_player_kimchristensen = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "kimchristensen",
    .name = "Kim Christensen",
    .version = "1",
    .extensions = "kim",
    .probe = kimchristensen_probe,
    .open = kimchristensen_open,
    .close = kimchristensen_close,
    .start = kimchristensen_start,
    .tick = kimchristensen_tick,
    .metadata = kimchristensen_metadata,
    .voice_info = kimchristensen_voice_info,
    .grid_ticks = kimchristensen_grid_ticks,
};

KimPlayer* kim_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_kimchristensen) {
        return nullptr;
    }
    return (KimPlayer*)nuade_player_data(song);
}
