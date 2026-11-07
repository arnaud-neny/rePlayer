// SPDX-License-Identifier: MIT
// Synth Pack as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/synthpack.h>
#include <stdio.h>

static NuadeProbe synthpack_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sp_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult synthpack_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SpCreateResult created = sp_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case SpStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message), "the sample set (smp.*) is missing");
                break;
            case SpStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Synth Pack song this port plays");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = SP_VOICES,
                                    .subsong_count = (uint32_t)sp_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = SP_TIMEBASE_HZ };
    return result;
}

static void synthpack_close(void* user_data) {
    sp_destroy((SpPlayer*)user_data);
}

static bool synthpack_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sp_start((SpPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult synthpack_tick(void* user_data, NuadeVoices* voices) {
    const SpTickResult tick = sp_tick((SpPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = SP_TICK_LENGTH, .flags = flags };
}

static void synthpack_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Synth Pack");
    sink->set_tag(sink->ctx, NUADE_META_ARTIST, "Karsten Obarski");
}

static uint32_t synthpack_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SpPlayer* player = (const SpPlayer*)user_data;
    const uint32_t n = cap < SP_VOICES ? cap : SP_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SpVoiceState v = sp_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t synthpack_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return sp_grid_ticks((const SpPlayer*)user_data);
}

const NuadePlayer nuade_player_synthpack = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "synthpack",
    .name = "Synth Pack",
    .version = "1",
    .extensions = "osp",
    .probe = synthpack_probe,
    .open = synthpack_open,
    .close = synthpack_close,
    .start = synthpack_start,
    .tick = synthpack_tick,
    .metadata = synthpack_metadata,
    .voice_info = synthpack_voice_info,
    .grid_ticks = synthpack_grid_ticks,
};

SpPlayer* sp_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_synthpack) {
        return nullptr;
    }
    return (SpPlayer*)nuade_player_data(song);
}
