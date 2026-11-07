// SPDX-License-Identifier: MIT
// Art and Magic music driver (.aam) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/artandmagic.h>
#include <stdio.h>

// Ticks one row of the display grid spans: the voices run their own note lengths, so the grid is a display choice only.
#define AAM_GRID_TICKS 6

static NuadeProbe artandmagic_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return aam_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult artandmagic_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    AamPlayer* player = aam_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Art and Magic module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = AAM_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = AAM_TIMEBASE_HZ
    };
    return result;
}

static void artandmagic_close(void* user_data) {
    aam_destroy((AamPlayer*)user_data);
}

static bool artandmagic_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && aam_start((AamPlayer*)user_data, voices);
}

static NuadeTickResult artandmagic_tick(void* user_data, NuadeVoices* voices) {
    const AamTickResult tick = aam_tick((AamPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = AAM_TICK_LENGTH, .flags = flags };
}

static void artandmagic_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Art and Magic");
}

static uint32_t artandmagic_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const AamPlayer* player = (const AamPlayer*)user_data;
    const uint32_t count = cap < AAM_VOICES ? cap : AAM_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const AamVoiceState state = aam_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = state.note, .instrument = state.instrument };
    }
    return count;
}

static uint32_t artandmagic_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return AAM_GRID_TICKS;
}

const NuadePlayer nuade_player_artandmagic = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "artandmagic",
    .name = "Art and Magic",
    .version = "1",
    .extensions = "aam",
    .probe = artandmagic_probe,
    .open = artandmagic_open,
    .close = artandmagic_close,
    .start = artandmagic_start,
    .tick = artandmagic_tick,
    .metadata = artandmagic_metadata,
    .voice_info = artandmagic_voice_info,
    .grid_ticks = artandmagic_grid_ticks,
};

AamPlayer* aam_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_artandmagic) {
        return nullptr;
    }
    return (AamPlayer*)nuade_player_data(song);
}
