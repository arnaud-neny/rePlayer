// SPDX-License-Identifier: MIT
// Rob Hubbard ST as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/robhubbardst.h>
#include <stdio.h>

static NuadeProbe robhubbardst_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return rhst_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult robhubbardst_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    RhstPlayer* player = rhst_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Rob Hubbard ST module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = RHST_VOICES,
                                    .subsong_count = (uint32_t)rhst_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = RHST_TIMEBASE_HZ };
    return result;
}

static void robhubbardst_close(void* user_data) {
    rhst_destroy((RhstPlayer*)user_data);
}

static bool robhubbardst_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return rhst_start((RhstPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult robhubbardst_tick(void* user_data, NuadeVoices* voices) {
    const RhstTickResult tick = rhst_tick((RhstPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = RHST_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : 0) };
}

static void robhubbardst_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Rob Hubbard ST");
}

const NuadePlayer nuade_player_robhubbardst = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "robhubbardst",
    .name = "Rob Hubbard ST",
    .version = "1",
    .extensions = "rho",
    .probe = robhubbardst_probe,
    .open = robhubbardst_open,
    .close = robhubbardst_close,
    .start = robhubbardst_start,
    .tick = robhubbardst_tick,
    .metadata = robhubbardst_metadata,
};

RhstPlayer* rhst_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_robhubbardst) {
        return nullptr;
    }
    return (RhstPlayer*)nuade_player_data(song);
}
