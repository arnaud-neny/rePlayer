// SPDX-License-Identifier: MIT
// Professional Sound Artists as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/professionalsoundartists.h>
#include <stdio.h>

static NuadeProbe professionalsoundartists_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return psa_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult professionalsoundartists_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    PsaPlayer* player = psa_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Professional Sound Artists song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = PSA_VOICES,
                                    .subsong_count = psa_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = PSA_TIMEBASE_HZ };
    return result;
}

static void professionalsoundartists_close(void* user_data) {
    psa_destroy((PsaPlayer*)user_data);
}

static bool professionalsoundartists_start(void* user_data, uint32_t subsong, uint32_t sample_rate,
                                           NuadeVoices* voices) {
    (void)sample_rate;
    return psa_start((PsaPlayer*)user_data, subsong, voices);
}

static NuadeTickResult professionalsoundartists_tick(void* user_data, NuadeVoices* voices) {
    const PsaTickResult tick = psa_tick((PsaPlayer*)user_data, voices);
    NuadeTickFlags flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = PSA_TICK_LENGTH, .flags = flags };
}

static void professionalsoundartists_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const PsaPlayer* player = (const PsaPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Professional Sound Artists");
    if (psa_title(player)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, psa_title(player));
    }
}

static uint32_t professionalsoundartists_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PsaPlayer* player = (const PsaPlayer*)user_data;
    const uint32_t n = cap < PSA_VOICES ? cap : PSA_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const PsaVoiceState v = psa_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t professionalsoundartists_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return psa_grid_ticks((const PsaPlayer*)user_data);
}

const NuadePlayer nuade_player_professionalsoundartists = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "professionalsoundartists",
    .name = "Professional Sound Artists",
    .version = "1",
    .extensions = "psa",
    .probe = professionalsoundartists_probe,
    .open = professionalsoundartists_open,
    .close = professionalsoundartists_close,
    .start = professionalsoundartists_start,
    .tick = professionalsoundartists_tick,
    .metadata = professionalsoundartists_metadata,
    .voice_info = professionalsoundartists_voice_info,
    .grid_ticks = professionalsoundartists_grid_ticks,
};

PsaPlayer* psa_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_professionalsoundartists) {
        return nullptr;
    }
    return (PsaPlayer*)nuade_player_data(song);
}
