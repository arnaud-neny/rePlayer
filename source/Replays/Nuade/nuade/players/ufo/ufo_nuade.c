// SPDX-License-Identifier: MIT
// UFO player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/ufo.h>
#include <stdio.h>

static NuadeProbe ufo_probe_song(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ufo_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult ufo_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const UfoCreateResult created = ufo_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case UfoStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            case UfoStatus_MissingBank:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "the sample bank (\"<song>.BANK\" or \"SMP.set\") is missing");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a UFO song this player knows");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) {
        .voice_count = UFO_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = UFO_TIMEBASE_HZ
    };
    return result;
}

static void ufo_close(void* user_data) {
    ufo_destroy((UfoPlayer*)user_data);
}

static bool ufo_start_song(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ufo_start((UfoPlayer*)user_data, voices);
}

static NuadeTickResult ufo_tick_song(void* user_data, NuadeVoices* voices) {
    const UfoTickResult tick = ufo_tick((UfoPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = UFO_TICK_LENGTH, .flags = flags };
}

static void ufo_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "UFO");
}

static uint32_t ufo_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const UfoPlayer* player = (const UfoPlayer*)user_data;
    const uint32_t n = cap < UFO_VOICES ? cap : UFO_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const UfoVoiceState v = ufo_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_ufo = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "ufo",
    .name = "UFO",
    .version = "1",
    .extensions = "mus",
    .probe = ufo_probe_song,
    .open = ufo_open,
    .close = ufo_close,
    .start = ufo_start_song,
    .tick = ufo_tick_song,
    .metadata = ufo_metadata,
    .voice_info = ufo_voice_info,
};

UfoPlayer* ufo_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_ufo) {
        return nullptr;
    }
    return (UfoPlayer*)nuade_player_data(song);
}
