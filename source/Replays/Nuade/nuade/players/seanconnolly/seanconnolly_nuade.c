// SPDX-License-Identifier: MIT
// Sean Connolly as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/seanconnolly.h>
#include <stdio.h>

#define SCO_GRID_TICKS 6 // the voices run on their own durations: a display grid only

static NuadeProbe seanconnolly_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sco_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult seanconnolly_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ScoPlayer* player = sco_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Sean Connolly song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = SCO_VOICES,
                                    .subsong_count = sco_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = SCO_TIMEBASE_HZ };
    return result;
}

static void seanconnolly_close(void* user_data) {
    sco_destroy((ScoPlayer*)user_data);
}

static bool seanconnolly_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sco_start((ScoPlayer*)user_data, subsong, voices);
}

static NuadeTickResult seanconnolly_tick(void* user_data, NuadeVoices* voices) {
    const ScoTickResult tick = sco_tick((ScoPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    flags |= tick.looped ? NuadeTick_Looped : 0u;
    flags |= tick.ended ? NuadeTick_Ended : 0u;
    return (NuadeTickResult) { .length = SCO_TICK_LENGTH, .flags = flags };
}

static void seanconnolly_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const ScoPlayer* player = (const ScoPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Sean Connolly");
    sink->set_tag(sink->ctx, NUADE_META_TITLE, sco_version_name(player));
}

static uint32_t seanconnolly_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ScoPlayer* player = (const ScoPlayer*)user_data;
    const uint32_t n = cap < SCO_VOICES ? cap : SCO_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const ScoVoiceState v = sco_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t seanconnolly_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return SCO_GRID_TICKS;
}

const NuadePlayer nuade_player_seanconnolly = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "seanconnolly",
    .name = "Sean Connolly",
    .version = "1",
    .extensions = "scn,s-c",
    .probe = seanconnolly_probe,
    .open = seanconnolly_open,
    .close = seanconnolly_close,
    .start = seanconnolly_start,
    .tick = seanconnolly_tick,
    .metadata = seanconnolly_metadata,
    .voice_info = seanconnolly_voice_info,
    .grid_ticks = seanconnolly_grid_ticks,
};

ScoPlayer* sco_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_seanconnolly) {
        return nullptr;
    }
    return (ScoPlayer*)nuade_player_data(song);
}
