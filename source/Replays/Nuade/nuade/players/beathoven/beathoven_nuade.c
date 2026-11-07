// SPDX-License-Identifier: MIT
// Beathoven Synthesizer as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/beathoven.h>
#include <stdio.h>

static NuadeProbe beathoven_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return bth_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult beathoven_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    BthPlayer* player = bth_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message),
                 "not a Beathoven Synthesizer module this player knows");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = BTH_VOICES,
                                    .subsong_count = bth_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = BTH_TIMEBASE_HZ };
    return result;
}

static void beathoven_close(void* user_data) {
    bth_destroy((BthPlayer*)user_data);
}

static bool beathoven_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return bth_start((BthPlayer*)user_data, subsong, voices);
}

static NuadeTickResult beathoven_tick(void* user_data, NuadeVoices* voices) {
    BthPlayer* player = (BthPlayer*)user_data;
    const BthTickResult tick = bth_tick(player, voices);
    return (NuadeTickResult) { .length = bth_tick_length(player),
                               .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void beathoven_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const BthPlayer* player = (const BthPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Beathoven Synthesizer");
    sink->set_tag(sink->ctx, NUADE_META_TITLE, bth_title(player));
    sink->set_tag(sink->ctx, NUADE_META_ARTIST, bth_author(player));
}

static uint32_t beathoven_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const BthPlayer* player = (const BthPlayer*)user_data;
    const uint32_t n = cap < BTH_VOICES ? cap : BTH_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const BthVoiceState v = bth_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t beathoven_grid_ticks(void* user_data, uint32_t subsong) {
    return bth_ticks_per_step((const BthPlayer*)user_data, subsong);
}

const NuadePlayer nuade_player_beathoven = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "beathoven",
    .name = "Beathoven Synthesizer",
    .version = "1",
    .extensions = "bss",
    .probe = beathoven_probe,
    .open = beathoven_open,
    .close = beathoven_close,
    .start = beathoven_start,
    .tick = beathoven_tick,
    .metadata = beathoven_metadata,
    .voice_info = beathoven_voice_info,
    .grid_ticks = beathoven_grid_ticks,
};

BthPlayer* bth_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_beathoven) {
        return nullptr;
    }
    return (BthPlayer*)nuade_player_data(song);
}
