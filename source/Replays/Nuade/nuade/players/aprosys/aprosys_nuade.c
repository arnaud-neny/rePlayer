// SPDX-License-Identifier: MIT
// AProSys as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/aprosys.h>
#include <stdio.h>

// Ticks one row of the display grid spans: rows have their own lengths (step duration times the speed command), so the
// grid is a display choice only.
#define APS_GRID_TICKS 6

static NuadeProbe aprosys_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return aps_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult aprosys_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ApsPlayer* player = aps_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid AProSys module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = APS_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = APS_TIMEBASE_HZ
    };
    return result;
}

static void aprosys_close(void* user_data) {
    aps_destroy((ApsPlayer*)user_data);
}

static bool aprosys_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return aps_start((ApsPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult aprosys_tick(void* user_data, NuadeVoices* voices) {
    const ApsTickResult tick = aps_tick((ApsPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = APS_TICK_LENGTH, .flags = flags };
}

static void aprosys_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "AProSys");
}

static uint32_t aprosys_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ApsPlayer* player = (const ApsPlayer*)user_data;
    const uint32_t count = cap < APS_VOICES ? cap : APS_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const ApsVoiceState state = aps_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = state.note, .instrument = state.instrument };
    }
    return count;
}

static uint32_t aprosys_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return APS_GRID_TICKS;
}

const NuadePlayer nuade_player_aprosys = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "aprosys",
    .name = "AProSys",
    .version = "1",
    .extensions = "aps",
    .probe = aprosys_probe,
    .open = aprosys_open,
    .close = aprosys_close,
    .start = aprosys_start,
    .tick = aprosys_tick,
    .metadata = aprosys_metadata,
    .voice_info = aprosys_voice_info,
    .grid_ticks = aprosys_grid_ticks,
};

ApsPlayer* aps_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_aprosys) {
        return nullptr;
    }
    return (ApsPlayer*)nuade_player_data(song);
}
