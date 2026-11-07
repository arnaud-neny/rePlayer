// SPDX-License-Identifier: MIT
// Mark Cooksey as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/markcooksey.h>
#include <stdio.h>

// The grid of the pattern view: the voices play on their own timing.
#define GRID_TICKS 6

static NuadeProbe markcooksey_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return mc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult markcooksey_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    McPlayer* player = mc_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Mark Cooksey module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = MC_VOICES,
                                    .subsong_count = (uint32_t)mc_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = MC_TIMEBASE_HZ };
    return result;
}

static void markcooksey_close(void* user_data) {
    mc_destroy((McPlayer*)user_data);
}

static bool markcooksey_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return mc_start((McPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult markcooksey_tick(void* user_data, NuadeVoices* voices) {
    const McTickResult tick = mc_tick((McPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = MC_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : 0) };
}

static void markcooksey_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Mark Cooksey");
}

static uint32_t markcooksey_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const McPlayer* player = (const McPlayer*)user_data;
    const uint32_t n = cap < MC_VOICES ? cap : MC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const McVoiceState v = mc_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t markcooksey_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_markcooksey = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "markcooksey",
    .name = "Mark Cooksey",
    .version = "1",
    .extensions = "mc",
    .probe = markcooksey_probe,
    .open = markcooksey_open,
    .close = markcooksey_close,
    .start = markcooksey_start,
    .tick = markcooksey_tick,
    .metadata = markcooksey_metadata,
    .voice_info = markcooksey_voice_info,
    .grid_ticks = markcooksey_grid_ticks,
};

McPlayer* mc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_markcooksey) {
        return nullptr;
    }
    return (McPlayer*)nuade_player_data(song);
}
