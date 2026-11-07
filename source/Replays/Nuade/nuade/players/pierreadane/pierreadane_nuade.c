// SPDX-License-Identifier: MIT
// Pierre Adane Packer as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/pierreadane.h>
#include <stdio.h>

#define PA_GRID_TICKS 3 // the voices run on their own durations: a display grid only, the replayer starts at speed 3

static NuadeProbe pierreadane_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return pa_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult pierreadane_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const PaCreateResult created = pa_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case PaStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "the \"smp.set\" sample file is missing or too short");
                break;
            case PaStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Pierre Adane Packer song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) {
        .voice_count = PA_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = PA_TIMEBASE_HZ
    };
    return result;
}

static void pierreadane_close(void* user_data) {
    pa_destroy((PaPlayer*)user_data);
}

static bool pierreadane_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return pa_start((PaPlayer*)user_data, voices);
}

static NuadeTickResult pierreadane_tick(void* user_data, NuadeVoices* voices) {
    const PaTickResult tick = pa_tick((PaPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    flags |= tick.looped ? NuadeTick_Looped : 0u;
    flags |= tick.ended ? NuadeTick_Ended : 0u;
    return (NuadeTickResult) { .length = PA_TICK_LENGTH, .flags = flags };
}

static void pierreadane_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Pierre Adane Packer");
}

static uint32_t pierreadane_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PaPlayer* player = (const PaPlayer*)user_data;
    const uint32_t n = cap < PA_VOICES ? cap : PA_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const PaVoiceState v = pa_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t pierreadane_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return PA_GRID_TICKS;
}

const NuadePlayer nuade_player_pierreadane = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "pierreadane",
    .name = "Pierre Adane Packer",
    .version = "1",
    .extensions = "pap",
    .probe = pierreadane_probe,
    .open = pierreadane_open,
    .close = pierreadane_close,
    .start = pierreadane_start,
    .tick = pierreadane_tick,
    .metadata = pierreadane_metadata,
    .voice_info = pierreadane_voice_info,
    .grid_ticks = pierreadane_grid_ticks,
};

PaPlayer* pa_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_pierreadane) {
        return nullptr;
    }
    return (PaPlayer*)nuade_player_data(song);
}
