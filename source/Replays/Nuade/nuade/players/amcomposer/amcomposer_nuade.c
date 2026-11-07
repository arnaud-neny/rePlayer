// SPDX-License-Identifier: MIT
// AM-Composer as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/amcomposer.h>
#include <stdio.h>

// The "AMC V1.2 REPLAY!" mark is specific enough to be sure.
static NuadeProbe amcomposer_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return amc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult amcomposer_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    AmcPlayer* player = amc_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid AM-Composer module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = AMC_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = AMC_TIMEBASE_HZ
    };
    return result;
}

static void amcomposer_close(void* user_data) {
    amc_destroy((AmcPlayer*)user_data);
}

static bool amcomposer_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return amc_start((AmcPlayer*)user_data, voices);
}

static NuadeTickResult amcomposer_tick(void* user_data, NuadeVoices* voices) {
    const AmcTickResult tick = amc_tick((AmcPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void amcomposer_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "AM-Composer");
    const int count = amc_instrument_count((const AmcPlayer*)user_data);
    for (int i = 0; i < count; i++) {
        char name[24];
        snprintf(name, sizeof(name), "Sample %d", i + 1);
        sink->add_instrument(sink->ctx, (uint32_t)i, name);
    }
}

static uint32_t amcomposer_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const AmcPlayer* player = (const AmcPlayer*)user_data;
    const uint32_t n = cap < AMC_VOICES ? cap : AMC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const AmcVoiceState v = amc_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument - 1 };
    }
    return n;
}

// The voices have their own streams; a step (every fourth tick) is the grid row.
static uint32_t amcomposer_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return AMC_TICKS_PER_STEP;
}

const NuadePlayer nuade_player_amcomposer = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "amcomposer",
    .name = "AM-Composer",
    .version = "1",
    .extensions = "amc",
    .probe = amcomposer_probe,
    .open = amcomposer_open,
    .close = amcomposer_close,
    .start = amcomposer_start,
    .tick = amcomposer_tick,
    .metadata = amcomposer_metadata,
    .voice_info = amcomposer_voice_info,
    .grid_ticks = amcomposer_grid_ticks,
};

AmcPlayer* amc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_amcomposer) {
        return nullptr;
    }
    return (AmcPlayer*)nuade_player_data(song);
}
