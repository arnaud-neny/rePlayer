// SPDX-License-Identifier: MIT
// Game Music Creator as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/gmc.h>
#include <stdio.h>

#define GMC_NUADE_VOICES 4
#define GMC_TICK_HZ 50
#define GMC_ROWS 64

// The format has no mark: a file that passes the layout checks is only "unsure".
static NuadeProbe gmc_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return gmc_probe(data, size) ? NuadeProbe_Unsure : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult gmc_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    GmcModule* module = gmc_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Game Music Creator module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = GMC_NUADE_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = GMC_TICK_HZ
    };
    return result;
}

static void gmc_nuade_close(void* user_data) {
    gmc_destroy((GmcModule*)user_data);
}

static bool gmc_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return gmc_start((GmcModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult gmc_nuade_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = gmc_tick((GmcModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void gmc_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Game Music Creator");
}

static NuadePositionResult gmc_nuade_position(void* user_data) {
    const GmcPosition p = gmc_position((const GmcModule*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t gmc_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const GmcModule* module = (const GmcModule*)user_data;
    const GmcPosition p = gmc_position(module);
    const uint32_t n = cap < GMC_NUADE_VOICES ? cap : GMC_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const GmcVoiceState v = gmc_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period),
                                    .instrument = v.sample != 0 ? v.sample - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult gmc_nuade_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= gmc_position_count((const GmcModule*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = GMC_NUADE_VOICES,
                                                .rows = GMC_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "inst", NuadeColumn_Hex, 1 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell gmc_nuade_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const GmcCell c = gmc_pattern_cell((const GmcModule*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = nuade_period_to_note(c.period);
    if (c.sample != 0) {
        cell.value[1] = c.sample;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t gmc_nuade_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if ((int)at->order >= gmc_position_count((const GmcModule*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, GMC_NUADE_VOICES, GMC_ROWS, channel, row_lo, row_hi, out, cap, gmc_nuade_cell,
                              user_data);
}

const NuadePlayer nuade_player_gmc = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "gmc",
    .name = "Game Music Creator",
    .version = "1",
    .extensions = "gmc",
    .probe = gmc_nuade_probe,
    .open = gmc_nuade_open,
    .close = gmc_nuade_close,
    .start = gmc_nuade_start,
    .tick = gmc_nuade_tick,
    .metadata = gmc_nuade_metadata,
    .position = gmc_nuade_position,
    .voice_info = gmc_nuade_voice_info,
    .tracker_info = gmc_nuade_tracker_info,
    .tracker_cells = gmc_nuade_tracker_cells,
};

GmcModule* gmc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_gmc) {
        return nullptr;
    }
    return (GmcModule*)nuade_player_data(song);
}
