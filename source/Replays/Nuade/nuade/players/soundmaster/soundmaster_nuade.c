// SPDX-License-Identifier: MIT
// Sound Master player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/soundmaster.h>
#include <stdio.h>

static NuadeProbe soundmaster_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return smst_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult soundmaster_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SmstPlayer* player = smst_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Sound Master module this player knows");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = SMST_VOICES,
                                    .subsong_count = (uint32_t)smst_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = SMST_TIMEBASE_HZ };
    return result;
}

static void soundmaster_close(void* user_data) {
    smst_destroy((SmstPlayer*)user_data);
}

static bool soundmaster_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    (void)voices;
    return smst_start((SmstPlayer*)user_data, (int)subsong);
}

static NuadeTickResult soundmaster_tick(void* user_data, NuadeVoices* voices) {
    const SmstTickResult tick = smst_tick((SmstPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SMST_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void soundmaster_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Sound Master");
}

static uint32_t soundmaster_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SmstPlayer* player = (const SmstPlayer*)user_data;
    const uint32_t count = cap < SMST_VOICES ? cap : SMST_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const SmstVoiceState v = smst_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t soundmaster_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)smst_ticks_per_row((const SmstPlayer*)user_data);
}

const NuadePlayer nuade_player_soundmaster = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "soundmaster",
    .name = "Sound Master",
    .version = "1",
    .extensions = "sm,sm1,sm2,sm3,smpro",
    .probe = soundmaster_probe,
    .open = soundmaster_open,
    .close = soundmaster_close,
    .start = soundmaster_start,
    .tick = soundmaster_tick,
    .metadata = soundmaster_metadata,
    .voice_info = soundmaster_voice_info,
    .grid_ticks = soundmaster_grid_ticks,
};

SmstPlayer* smst_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_soundmaster) {
        return nullptr;
    }
    return (SmstPlayer*)nuade_player_data(song);
}
