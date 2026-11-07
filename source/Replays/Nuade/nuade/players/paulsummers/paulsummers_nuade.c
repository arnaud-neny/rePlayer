// SPDX-License-Identifier: MIT
// Paul Summers as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/paulsummers.h>
#include <stdio.h>

static NuadeProbe paulsummers_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ps_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult paulsummers_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    PsPlayer* player = ps_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Paul Summers song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = PS_VOICES,
                                    .subsong_count = ps_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = PS_TIMEBASE_HZ };
    return result;
}

static void paulsummers_close(void* user_data) {
    ps_destroy((PsPlayer*)user_data);
}

static bool paulsummers_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return ps_start((PsPlayer*)user_data, subsong, voices);
}

static NuadeTickResult paulsummers_tick(void* user_data, NuadeVoices* voices) {
    const PsTickResult tick = ps_tick((PsPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = tick.length, .flags = flags };
}

static void paulsummers_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Paul Summers");
}

static uint32_t paulsummers_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PsPlayer* player = (const PsPlayer*)user_data;
    const uint32_t n = cap < PS_VOICES ? cap : PS_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const PsVoiceState v = ps_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_paulsummers = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "paulsummers",
    .name = "Paul Summers",
    .version = "1",
    .extensions = "snk",
    .probe = paulsummers_probe,
    .open = paulsummers_open,
    .close = paulsummers_close,
    .start = paulsummers_start,
    .tick = paulsummers_tick,
    .metadata = paulsummers_metadata,
    .voice_info = paulsummers_voice_info,
};

PsPlayer* ps_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_paulsummers) {
        return nullptr;
    }
    return (PsPlayer*)nuade_player_data(song);
}
