// SPDX-License-Identifier: MIT
// Paul Shields player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/paulshields.h>
#include <stdio.h>

static NuadeProbe paulshields_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return psh_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult paulshields_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    PshPlayer* player = psh_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Paul Shields module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = PSH_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = PSH_TIMEBASE_HZ
    };
    return result;
}

static void paulshields_close(void* user_data) {
    psh_destroy((PshPlayer*)user_data);
}

static bool paulshields_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return psh_start((PshPlayer*)user_data, voices);
}

static NuadeTickResult paulshields_tick(void* user_data, NuadeVoices* voices) {
    const PshTickResult tick = psh_tick((PshPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = PSH_TICK_LENGTH, .flags = flags };
}

static void paulshields_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Paul Shields");
}

static uint32_t paulshields_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PshPlayer* player = (const PshPlayer*)user_data;
    const uint32_t count = cap < PSH_VOICES ? cap : PSH_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const PshVoiceState v = psh_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

const NuadePlayer nuade_player_paulshields = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "paulshields",
    .name = "Paul Shields",
    .version = "1",
    .extensions = "ps",
    .probe = paulshields_probe,
    .open = paulshields_open,
    .close = paulshields_close,
    .start = paulshields_start,
    .tick = paulshields_tick,
    .metadata = paulshields_metadata,
    .voice_info = paulshields_voice_info,
};

PshPlayer* psh_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_paulshields) {
        return nullptr;
    }
    return (PshPlayer*)nuade_player_data(song);
}
