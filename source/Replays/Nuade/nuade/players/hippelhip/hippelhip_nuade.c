// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/hippelhip.h>
#include <stdio.h>

static NuadeProbe hippelhip_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return hh_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult hippelhip_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    HhPlayer* player = hh_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Jochen Hippel .hip module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = HH_VOICES,
                                    .subsong_count = (uint32_t)hh_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = HH_TIMEBASE_HZ };
    return result;
}

static void hippelhip_close(void* user_data) {
    hh_destroy((HhPlayer*)user_data);
}

static bool hippelhip_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return hh_start((HhPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult hippelhip_tick(void* user_data, NuadeVoices* voices) {
    const HhTickResult tick = hh_tick((HhPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = HH_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void hippelhip_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jochen Hippel (TFMX header)");
    sink->set_tag(sink->ctx, NUADE_META_TOOL, hh_replayer_name((const HhPlayer*)user_data));
}

static uint32_t hippelhip_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const HhPlayer* player = (const HhPlayer*)user_data;
    const uint32_t n = cap < HH_VOICES ? cap : HH_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const HhVoiceState v = hh_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

// The voices run their own patterns but step together: one grid row is one sequencer step.
static uint32_t hippelhip_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)hh_speed((const HhPlayer*)user_data);
}

const NuadePlayer nuade_player_hippelhip = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "hippelhip",
    .name = "Jochen Hippel (.hip)",
    .version = "1",
    .extensions = "hip",
    .probe = hippelhip_probe,
    .open = hippelhip_open,
    .close = hippelhip_close,
    .start = hippelhip_start,
    .tick = hippelhip_tick,
    .metadata = hippelhip_metadata,
    .voice_info = hippelhip_voice_info,
    .grid_ticks = hippelhip_grid_ticks,
};

HhPlayer* hh_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_hippelhip) {
        return nullptr;
    }
    return (HhPlayer*)nuade_player_data(song);
}
