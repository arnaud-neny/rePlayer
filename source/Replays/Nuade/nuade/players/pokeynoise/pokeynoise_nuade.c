// SPDX-License-Identifier: MIT
// Pokeynoise as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/pokeynoise.h>
#include <stdio.h>

static NuadeProbe pokeynoise_probe(const uint8_t* data, size_t size, const char* filename) {
    return pn_probe(data, size, filename) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult pokeynoise_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    PnPlayer* player = pn_create(source->data, source->size, source->path, source->io);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Pokeynoise song (needs its .info file)");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = PN_VOICES,
                                    .subsong_count = pn_subsong_count(player),
                                    .default_subsong = pn_default_subsong(player),
                                    .timebase_hz = PN_TIMEBASE_HZ };
    return result;
}

static void pokeynoise_close(void* user_data) {
    pn_destroy((PnPlayer*)user_data);
}

static bool pokeynoise_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return pn_start((PnPlayer*)user_data, subsong, voices);
}

static NuadeTickResult pokeynoise_tick(void* user_data, NuadeVoices* voices) {
    const PnTickResult tick = pn_tick((PnPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = NuadeTick_None };
}

static void pokeynoise_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Pokeynoise");
}

static uint32_t pokeynoise_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PnPlayer* player = (const PnPlayer*)user_data;
    const uint32_t n = cap < PN_VOICES ? cap : PN_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const PnVoiceState v = pn_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = 0 };
    }
    return n;
}

const NuadePlayer nuade_player_pokeynoise = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "pokeynoise",
    .name = "Pokeynoise",
    .version = "1",
    .extensions = "pn",
    .probe = pokeynoise_probe,
    .open = pokeynoise_open,
    .close = pokeynoise_close,
    .start = pokeynoise_start,
    .tick = pokeynoise_tick,
    .metadata = pokeynoise_metadata,
    .voice_info = pokeynoise_voice_info,
};

PnPlayer* pn_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_pokeynoise) {
        return nullptr;
    }
    return (PnPlayer*)nuade_player_data(song);
}
