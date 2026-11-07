// SPDX-License-Identifier: MIT
// Rob Hubbard as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/robhubbard.h>
#include <stdio.h>

static NuadeProbe robhubbard_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return rh_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult robhubbard_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    RhPlayer* player = rh_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Rob Hubbard module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = RH_VOICES,
                                    .subsong_count = (uint32_t)rh_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = RH_TIMEBASE_HZ };
    return result;
}

static void robhubbard_close(void* user_data) {
    rh_destroy((RhPlayer*)user_data);
}

static bool robhubbard_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return rh_start((RhPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult robhubbard_tick(void* user_data, NuadeVoices* voices) {
    const RhTickResult tick = rh_tick((RhPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = RH_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : 0) };
}

static void robhubbard_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Rob Hubbard");
}

static uint32_t robhubbard_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const RhPlayer* player = (const RhPlayer*)user_data;
    const uint32_t n = cap < RH_VOICES ? cap : RH_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const RhVoiceState v = rh_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

// The voices play lengths of whole units of the subsong's tempo.
static uint32_t robhubbard_grid_ticks(void* user_data, uint32_t subsong) {
    const int tempo = rh_subsong_tempo((const RhPlayer*)user_data, (int)subsong);
    return tempo > 0 ? (uint32_t)tempo : 1;
}

const NuadePlayer nuade_player_robhubbard = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "robhubbard",
    .name = "Rob Hubbard",
    .version = "1",
    .extensions = "rh",
    .probe = robhubbard_probe,
    .open = robhubbard_open,
    .close = robhubbard_close,
    .start = robhubbard_start,
    .tick = robhubbard_tick,
    .metadata = robhubbard_metadata,
    .voice_info = robhubbard_voice_info,
    .grid_ticks = robhubbard_grid_ticks,
};

RhPlayer* rh_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_robhubbard) {
        return nullptr;
    }
    return (RhPlayer*)nuade_player_data(song);
}
