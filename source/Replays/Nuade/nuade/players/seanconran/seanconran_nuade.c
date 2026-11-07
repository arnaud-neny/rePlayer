// SPDX-License-Identifier: MIT
// Sean Conran as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/seanconran.h>
#include <stdio.h>

static NuadeProbe seanconran_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return scr_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult seanconran_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ScrPlayer* player = scr_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Sean Conran module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = SCR_VOICES,
                                    .subsong_count = (uint32_t)scr_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = SCR_TIMEBASE_HZ };
    return result;
}

static void seanconran_close(void* user_data) {
    scr_destroy((ScrPlayer*)user_data);
}

static bool seanconran_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return scr_start((ScrPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult seanconran_tick(void* user_data, NuadeVoices* voices) {
    const ScrTickResult tick = scr_tick((ScrPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SCR_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : 0) };
}

static void seanconran_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Sean Conran");
}

const NuadePlayer nuade_player_seanconran = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "seanconran",
    .name = "Sean Conran",
    .version = "1",
    .extensions = "scr",
    .probe = seanconran_probe,
    .open = seanconran_open,
    .close = seanconran_close,
    .start = seanconran_start,
    .tick = seanconran_tick,
    .metadata = seanconran_metadata,
};

ScrPlayer* scr_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_seanconran) {
        return nullptr;
    }
    return (ScrPlayer*)nuade_player_data(song);
}
