// SPDX-License-Identifier: MIT
// Medley player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/medley.h>
#include <stdio.h>

// Ticks one row of the display grid spans: the voices run their own tracks, so the grid is a display choice only.
#define MDL_GRID_TICKS 8

static NuadeProbe medley_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return mdl_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult medley_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    MdlPlayer* player = mdl_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Medley module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = MDL_VOICES,
                                    .subsong_count = (uint32_t)mdl_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = MDL_TIMEBASE_HZ };
    return result;
}

static void medley_close(void* user_data) {
    mdl_destroy((MdlPlayer*)user_data);
}

static bool medley_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return mdl_start((MdlPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult medley_tick(void* user_data, NuadeVoices* voices) {
    const MdlTickResult tick = mdl_tick((MdlPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = MDL_TICK_LENGTH, .flags = flags };
}

static void medley_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Medley");
}

static uint32_t medley_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const MdlPlayer* player = (const MdlPlayer*)user_data;
    const uint32_t count = cap < MDL_VOICES ? cap : MDL_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const MdlVoiceState state = mdl_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = state.note, .instrument = state.instrument };
    }
    return count;
}

static uint32_t medley_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return MDL_GRID_TICKS;
}

const NuadePlayer nuade_player_medley = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "medley",
    .name = "Medley",
    .version = "1",
    .extensions = "mso",
    .probe = medley_probe,
    .open = medley_open,
    .close = medley_close,
    .start = medley_start,
    .tick = medley_tick,
    .metadata = medley_metadata,
    .voice_info = medley_voice_info,
    .grid_ticks = medley_grid_ticks,
};

MdlPlayer* mdl_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_medley) {
        return nullptr;
    }
    return (MdlPlayer*)nuade_player_data(song);
}
