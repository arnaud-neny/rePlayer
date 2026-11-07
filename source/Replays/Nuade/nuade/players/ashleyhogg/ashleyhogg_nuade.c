// SPDX-License-Identifier: MIT
// Ashley Hogg game sound drivers as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/ashleyhogg.h>
#include <stdio.h>

#define MODULE_PREFIX "ash"

static NuadeProbe ashleyhogg_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ah_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult ashleyhogg_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const AhCreateResult created = ah_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        result.error.status
            = created.status == AhStatus_OutOfMemory ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        if (created.status == AhStatus_MissingSamples) {
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "the \"smp.\" sample file is missing");
        } else {
            snprintf(result.error.message, sizeof(result.error.message), "not a known Ashley Hogg driver");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = AH_VOICES,
                                    .subsong_count = (uint32_t)ah_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = AH_TIMEBASE_HZ };
    return result;
}

static void ashleyhogg_close(void* user_data) {
    ah_destroy((AhPlayer*)user_data);
}

static bool ashleyhogg_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return ah_start((AhPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult ashleyhogg_tick(void* user_data, NuadeVoices* voices) {
    const AhTickResult tick = ah_tick((AhPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = AH_TICK_LENGTH, .flags = flags };
}

static void ashleyhogg_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Ashley Hogg");
}

static uint32_t ashleyhogg_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const AhPlayer* player = (const AhPlayer*)user_data;
    const uint32_t n = cap < AH_VOICES ? cap : AH_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const AhVoiceState v = ah_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t ashleyhogg_grid_ticks(void* user_data, uint32_t subsong) {
    return ah_grid_ticks((const AhPlayer*)user_data, (int)subsong);
}

const NuadePlayer nuade_player_ashleyhogg = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "ashleyhogg",
    .name = "Ashley Hogg",
    .version = "1",
    .extensions = MODULE_PREFIX,
    .probe = ashleyhogg_probe,
    .open = ashleyhogg_open,
    .close = ashleyhogg_close,
    .start = ashleyhogg_start,
    .tick = ashleyhogg_tick,
    .metadata = ashleyhogg_metadata,
    .voice_info = ashleyhogg_voice_info,
    .grid_ticks = ashleyhogg_grid_ticks,
};

AhPlayer* ah_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_ashleyhogg) {
        return nullptr;
    }
    return (AhPlayer*)nuade_player_data(song);
}
