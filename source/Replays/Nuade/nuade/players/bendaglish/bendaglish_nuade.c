// SPDX-License-Identifier: MIT
// Ben Daglish (Amiga) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/bendaglish.h>
#include <stdio.h>
#include <stdlib.h>

#define BENDAGLISH_TICK_HZ 50 // PAL VBlank
#define BENDAGLISH_TICKS_PER_ROW 1

typedef struct BendaglishNuade {
    BdModule* module;
} BendaglishNuade;

// The check walks the player code the module starts with, so a match is certain.
static NuadeProbe bendaglish_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return bd_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static void bendaglish_close(void* user_data) {
    BendaglishNuade* player = (BendaglishNuade*)user_data;
    if (player == nullptr) {
        return;
    }
    bd_destroy(player->module);
    free(player);
}

static NuadePlayerOpenResult bendaglish_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    BendaglishNuade* player = (BendaglishNuade*)calloc(1, sizeof(BendaglishNuade));
    if (player == nullptr) {
        result.error.status = NuadeStatus_OutOfMemory;
        return result;
    }
    player->module = bd_create(source->data, source->size);
    if (player->module == nullptr) {
        free(player);
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Ben Daglish module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = (uint32_t)bd_channel_count(player->module),
        .subsong_count = (uint32_t)bd_subsong_count(player->module),
        .default_subsong = 0,
        .timebase_hz = BENDAGLISH_TICK_HZ,
    };
    return result;
}

static bool bendaglish_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return bd_start(((BendaglishNuade*)user_data)->module, (int)subsong, voices);
}

static NuadeTickResult bendaglish_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    const bool looped = bd_tick(((BendaglishNuade*)user_data)->module);
    return (NuadeTickResult) { .length = 1, .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void bendaglish_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Ben Daglish");
}

// A track note lasts any number of ticks (1 to 52 seen), so no larger row would show every start: a row is one tick.
static uint32_t bendaglish_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return BENDAGLISH_TICKS_PER_ROW;
}

static uint32_t bendaglish_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const BdModule* module = ((BendaglishNuade*)user_data)->module;
    const uint32_t count = (uint32_t)bd_channel_count(module);
    for (uint32_t i = 0; i < count && i < cap; i++) {
        out[i] = (NuadeVoiceInfo) { .note = -1, .instrument = -1 };
        bd_voice_started(module, i, &out[i].note, &out[i].instrument);
    }
    return count < cap ? count : cap;
}

const NuadePlayer nuade_player_bendaglish = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "bendaglish",
    .name = "Ben Daglish",
    .version = "1",
    .extensions = "bd,mth",
    .probe = bendaglish_probe,
    .open = bendaglish_open,
    .close = bendaglish_close,
    .start = bendaglish_start,
    .tick = bendaglish_tick,
    .metadata = bendaglish_metadata,
    .voice_info = bendaglish_voice_info,
    .grid_ticks = bendaglish_grid_ticks,
};

BdModule* bd_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_bendaglish) {
        return nullptr;
    }
    return ((BendaglishNuade*)nuade_player_data(song))->module;
}
