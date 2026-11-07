// SPDX-License-Identifier: MIT
// NTSP-system as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/ntsp.h>
#include <stdio.h>

static NuadeProbe ntsp_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ntsp_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult ntsp_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    NtspPlayer* player = ntsp_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid NTSP-system module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = NTSP_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = NTSP_TIMEBASE_HZ
    };
    return result;
}

static void ntsp_nuade_close(void* user_data) {
    ntsp_destroy((NtspPlayer*)user_data);
}

static bool ntsp_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return ntsp_start((NtspPlayer*)user_data, voices);
}

static NuadeTickResult ntsp_nuade_tick(void* user_data, NuadeVoices* voices) {
    const NtspTickResult tick = ntsp_tick((NtspPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = NTSP_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void ntsp_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "NTSP-system");
}

const NuadePlayer nuade_player_ntsp = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "ntsp",
    .name = "NTSP-system",
    .version = "1",
    .extensions = "two",
    .probe = ntsp_nuade_probe,
    .open = ntsp_nuade_open,
    .close = ntsp_nuade_close,
    .start = ntsp_nuade_start,
    .tick = ntsp_nuade_tick,
    .metadata = ntsp_nuade_metadata,
};

NtspPlayer* ntsp_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_ntsp) {
        return nullptr;
    }
    return (NtspPlayer*)nuade_player_data(song);
}
