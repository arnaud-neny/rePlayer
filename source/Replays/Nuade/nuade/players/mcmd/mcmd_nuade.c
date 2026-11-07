// SPDX-License-Identifier: MIT
// MCMD as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/mcmd.h>
#include <stdio.h>

// Period table index 12 (856) is C-1.
#define MCMD_NOTE_BASE 0

static NuadeProbe mcmd_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return mcmd_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult mcmd_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    McmdPlayer* player = mcmd_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid MCMD module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = MCMD_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = MCMD_TIMEBASE_HZ
    };
    return result;
}

static void mcmd_nuade_close(void* user_data) {
    mcmd_destroy((McmdPlayer*)user_data);
}

static bool mcmd_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && mcmd_start((McmdPlayer*)user_data, voices);
}

static NuadeTickResult mcmd_nuade_tick(void* user_data, NuadeVoices* voices) {
    const McmdTickResult tick = mcmd_tick((McmdPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = MCMD_TICK_LENGTH, .flags = flags };
}

static void mcmd_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "MCMD");
}

static uint32_t mcmd_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const McmdPlayer* player = (const McmdPlayer*)user_data;
    const uint32_t n = cap < MCMD_VOICES ? cap : MCMD_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const McmdVoiceState v = mcmd_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note >= 0 ? v.note + MCMD_NOTE_BASE : -1, .instrument = v.instrument };
    }
    return n;
}

// The voices run their own patterns: one grid row is one sequencer step.
static uint32_t mcmd_nuade_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)mcmd_speed((const McmdPlayer*)user_data);
}

const NuadePlayer nuade_player_mcmd = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "mcmd",
    .name = "MCMD",
    .version = "1",
    .extensions = "mcmd,mcmd_org",
    .probe = mcmd_nuade_probe,
    .open = mcmd_nuade_open,
    .close = mcmd_nuade_close,
    .start = mcmd_nuade_start,
    .tick = mcmd_nuade_tick,
    .metadata = mcmd_nuade_metadata,
    .voice_info = mcmd_nuade_voice_info,
    .grid_ticks = mcmd_nuade_grid_ticks,
};

McmdPlayer* mcmd_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_mcmd) {
        return nullptr;
    }
    return (McmdPlayer*)nuade_player_data(song);
}
