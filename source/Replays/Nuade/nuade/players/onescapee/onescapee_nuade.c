// SPDX-License-Identifier: MIT
// onEscapee as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/onescapee.h>
#include <stdio.h>

static NuadeProbe onescapee_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return onescapee_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult onescapee_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    OnEscapeePlayer* player = onescapee_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid onEscapee module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = ONESCAPEE_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = ONESCAPEE_TIMEBASE_HZ
    };
    return result;
}

static void onescapee_nuade_close(void* user_data) {
    onescapee_destroy((OnEscapeePlayer*)user_data);
}

static bool onescapee_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && onescapee_start((OnEscapeePlayer*)user_data, voices);
}

static NuadeTickResult onescapee_nuade_tick(void* user_data, NuadeVoices* voices) {
    const OnEscapeeTickResult tick = onescapee_tick((OnEscapeePlayer*)user_data, voices);
    return (NuadeTickResult) { .length = ONESCAPEE_TICK_LENGTH,
                               .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void onescapee_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "onEscapee");
}

const NuadePlayer nuade_player_onescapee = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "onescapee",
    .name = "onEscapee",
    .version = "1",
    .extensions = "one",
    .probe = onescapee_nuade_probe,
    .open = onescapee_nuade_open,
    .close = onescapee_nuade_close,
    .start = onescapee_nuade_start,
    .tick = onescapee_nuade_tick,
    .metadata = onescapee_nuade_metadata,
};

OnEscapeePlayer* onescapee_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_onescapee) {
        return nullptr;
    }
    return (OnEscapeePlayer*)nuade_player_data(song);
}
