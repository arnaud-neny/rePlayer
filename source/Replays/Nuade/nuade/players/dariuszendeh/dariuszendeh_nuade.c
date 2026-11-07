// SPDX-License-Identifier: MIT
// Darius Zendeh as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/dariuszendeh.h>
#include <stdio.h>

static NuadeProbe dariuszendeh_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dz_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult dariuszendeh_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DzPlayer* player = dz_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Darius Zendeh module this player knows");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = DZ_VOICES,
                                    .subsong_count = (uint32_t)dz_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = DZ_TIMEBASE_HZ };
    return result;
}

static void dariuszendeh_close(void* user_data) {
    dz_destroy((DzPlayer*)user_data);
}

static bool dariuszendeh_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dz_start((DzPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult dariuszendeh_tick(void* user_data, NuadeVoices* voices) {
    const DzTickResult tick = dz_tick((DzPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = DZ_TICK_LENGTH, .flags = flags };
}

static void dariuszendeh_metadata(void* user_data, const NuadeMetadataSink* sink) {
    char format[48];
    snprintf(format, sizeof(format), "Darius Zendeh Mod - type: %c", dz_version((const DzPlayer*)user_data));
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, format);
}

static uint32_t dariuszendeh_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DzPlayer* player = (const DzPlayer*)user_data;
    const uint32_t n = cap < DZ_VOICES ? cap : DZ_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const DzVoiceState v = dz_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t dariuszendeh_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return DZ_TICKS_PER_ROW;
}

const NuadePlayer nuade_player_dariuszendeh = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "dariuszendeh",
    .name = "Darius Zendeh",
    .version = "1",
    .extensions = "dz,dzp,mkiio",
    .probe = dariuszendeh_probe,
    .open = dariuszendeh_open,
    .close = dariuszendeh_close,
    .start = dariuszendeh_start,
    .tick = dariuszendeh_tick,
    .metadata = dariuszendeh_metadata,
    .voice_info = dariuszendeh_voice_info,
    .grid_ticks = dariuszendeh_grid_ticks,
};

DzPlayer* dz_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_dariuszendeh) {
        return nullptr;
    }
    return (DzPlayer*)nuade_player_data(song);
}
