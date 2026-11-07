// SPDX-License-Identifier: MIT
// Riff Raff as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/riffraff.h>
#include <stdio.h>

#define RR_GRID_TICKS 6 // the voices run on their own durations: a display grid only

static NuadeProbe riffraff_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return rr_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult riffraff_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    RrPlayer* player = rr_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Riff Raff song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = RR_VOICES,
                                    .subsong_count = rr_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = RR_TIMEBASE_HZ };
    return result;
}

static void riffraff_close(void* user_data) {
    rr_destroy((RrPlayer*)user_data);
}

static bool riffraff_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return rr_start((RrPlayer*)user_data, subsong, voices);
}

static NuadeTickResult riffraff_tick(void* user_data, NuadeVoices* voices) {
    const RrTickResult tick = rr_tick((RrPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = RR_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void riffraff_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const RrPlayer* player = (const RrPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Riff Raff");
    if (rr_title(player)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, rr_title(player));
    }
}

static uint32_t riffraff_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const RrPlayer* player = (const RrPlayer*)user_data;
    const uint32_t n = cap < RR_VOICES ? cap : RR_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const RrVoiceState v = rr_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t riffraff_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return RR_GRID_TICKS;
}

const NuadePlayer nuade_player_riffraff = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "riffraff",
    .name = "Riff Raff",
    .version = "1",
    .extensions = "riff",
    .probe = riffraff_probe,
    .open = riffraff_open,
    .close = riffraff_close,
    .start = riffraff_start,
    .tick = riffraff_tick,
    .metadata = riffraff_metadata,
    .voice_info = riffraff_voice_info,
    .grid_ticks = riffraff_grid_ticks,
};

RrPlayer* rr_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_riffraff) {
        return nullptr;
    }
    return (RrPlayer*)nuade_player_data(song);
}
