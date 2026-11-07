// SPDX-License-Identifier: MIT
// Future Composer BSI as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/fcbsi.h>

static NuadeProbe fcbsi_probe_file(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return fcbsi_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult fcbsi_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    FcBsiPlayer* player = fcbsi_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status
            = fcbsi_probe(source->data, source->size) ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = FCBSI_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = FCBSI_TIMEBASE_HZ
    };
    return result;
}

static void fcbsi_close(void* user_data) {
    fcbsi_destroy((FcBsiPlayer*)user_data);
}

static bool fcbsi_start_song(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && fcbsi_start((FcBsiPlayer*)user_data, voices);
}

static NuadeTickResult fcbsi_tick_song(void* user_data, NuadeVoices* voices) {
    const FcBsiTickResult tick = fcbsi_tick((FcBsiPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = FCBSI_TICK_LENGTH, .flags = tick.ended ? NuadeTick_Ended : NuadeTick_None };
}

static void fcbsi_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Future Composer BSI");
}

static uint32_t fcbsi_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const FcBsiPlayer* player = (const FcBsiPlayer*)user_data;
    const uint32_t n = cap < FCBSI_VOICES ? cap : FCBSI_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const FcBsiVoiceState v = fcbsi_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t fcbsi_grid_ticks_song(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return fcbsi_grid_ticks();
}

const NuadePlayer nuade_player_fcbsi = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "fcbsi",
    .name = "Future Composer BSI",
    .version = "1",
    .extensions = "bsi,bfc",
    .probe = fcbsi_probe_file,
    .open = fcbsi_open,
    .close = fcbsi_close,
    .start = fcbsi_start_song,
    .tick = fcbsi_tick_song,
    .metadata = fcbsi_metadata,
    .voice_info = fcbsi_voice_info,
    .grid_ticks = fcbsi_grid_ticks_song,
};

FcBsiPlayer* fcbsi_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_fcbsi) {
        return nullptr;
    }
    return (FcBsiPlayer*)nuade_player_data(song);
}
