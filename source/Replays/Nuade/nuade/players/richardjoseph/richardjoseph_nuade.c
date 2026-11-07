// SPDX-License-Identifier: MIT
// Richard Joseph Player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/richardjoseph.h>
#include <stdio.h>

static NuadeProbe richardjoseph_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return rjp_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult richardjoseph_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const RjpCreateResult created = rjp_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case RjpStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            case RjpStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "the sample file (smp.name, name.ins or smp.set) is missing");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Richard Joseph Player song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = RJP_VOICES,
                                    .subsong_count = rjp_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = RJP_TIMEBASE_HZ };
    return result;
}

static void richardjoseph_close(void* user_data) {
    rjp_destroy((RjpPlayer*)user_data);
}

static bool richardjoseph_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return rjp_start((RjpPlayer*)user_data, subsong, voices);
}

static NuadeTickResult richardjoseph_tick(void* user_data, NuadeVoices* voices) {
    const RjpTickResult tick = rjp_tick((RjpPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = RJP_TICK_LENGTH, .flags = flags };
}

static void richardjoseph_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Richard Joseph Player");
}

static uint32_t richardjoseph_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const RjpPlayer* player = (const RjpPlayer*)user_data;
    const uint32_t n = cap < RJP_VOICES ? cap : RJP_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const RjpVoiceState v = rjp_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t richardjoseph_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return rjp_grid_ticks();
}

const NuadePlayer nuade_player_richardjoseph = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "richardjoseph",
    .name = "Richard Joseph Player",
    .version = "1",
    .extensions = "rjp,sng",
    .probe = richardjoseph_probe,
    .open = richardjoseph_open,
    .close = richardjoseph_close,
    .start = richardjoseph_start,
    .tick = richardjoseph_tick,
    .metadata = richardjoseph_metadata,
    .voice_info = richardjoseph_voice_info,
    .grid_ticks = richardjoseph_grid_ticks,
};

RjpPlayer* rjp_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_richardjoseph) {
        return nullptr;
    }
    return (RjpPlayer*)nuade_player_data(song);
}
