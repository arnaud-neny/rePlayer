// SPDX-License-Identifier: MIT
// Forgotten Worlds as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/forgottenworlds.h>
#include <stdio.h>

// The grid of the pattern view: the voices play on their own timing.
#define GRID_TICKS 6

static NuadeProbe forgottenworlds_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return fw_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult forgottenworlds_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    FwPlayer* player = fw_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Forgotten Worlds module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = FW_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = FW_TIMEBASE_HZ
    };
    return result;
}

static void forgottenworlds_close(void* user_data) {
    fw_destroy((FwPlayer*)user_data);
}

static bool forgottenworlds_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    if (subsong != 0) {
        return false;
    }
    fw_start((FwPlayer*)user_data, voices);
    return true;
}

static NuadeTickResult forgottenworlds_tick(void* user_data, NuadeVoices* voices) {
    const FwTickResult tick = fw_tick((FwPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = FW_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : NuadeTick_None) };
}

static void forgottenworlds_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Forgotten Worlds");
}

static uint32_t forgottenworlds_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const FwPlayer* player = (const FwPlayer*)user_data;
    const uint32_t n = cap < FW_VOICES ? cap : FW_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const FwVoiceState v = fw_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t forgottenworlds_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_forgottenworlds = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "forgottenworlds",
    .name = "Forgotten Worlds",
    .version = "1",
    .extensions = "fw",
    .probe = forgottenworlds_probe,
    .open = forgottenworlds_open,
    .close = forgottenworlds_close,
    .start = forgottenworlds_start,
    .tick = forgottenworlds_tick,
    .metadata = forgottenworlds_metadata,
    .voice_info = forgottenworlds_voice_info,
    .grid_ticks = forgottenworlds_grid_ticks,
};

FwPlayer* fw_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_forgottenworlds) {
        return nullptr;
    }
    return (FwPlayer*)nuade_player_data(song);
}
