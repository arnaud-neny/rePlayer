// SPDX-License-Identifier: MIT
// Digital Mugician / Digital Mugician 2 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/digmug.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct DigmugNuade {
    DmModule* module;
} DigmugNuade;

// The module mark is specific enough to be sure.
static NuadeProbe digmug_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dm_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult digmug_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DmModule* module = dm_create(source->data, source->size);
    if (module == nullptr || dm_subsong_count(module) <= 0) {
        dm_destroy(module);
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Digital Mugician module");
        return result;
    }
    DigmugNuade* state = (DigmugNuade*)calloc(1, sizeof(DigmugNuade));
    if (state == nullptr) {
        dm_destroy(module);
        result.error.status = NuadeStatus_OutOfMemory;
        snprintf(result.error.message, sizeof(result.error.message), "out of memory");
        return result;
    }
    state->module = module;
    result.user_data = state;
    result.info = (NuadeSongInfo) { .voice_count = (uint32_t)dm_channel_count(module),
                                    .subsong_count = (uint32_t)dm_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = dm_timebase_hz(module) };
    return result;
}

static void digmug_close(void* user_data) {
    DigmugNuade* state = (DigmugNuade*)user_data;
    dm_destroy(state->module);
    free(state);
}

static bool digmug_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dm_start(((DigmugNuade*)user_data)->module, (int)subsong, voices);
}

static NuadeTickResult digmug_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    DmModule* module = ((DigmugNuade*)user_data)->module;
    const bool looped = dm_tick(module);
    return (NuadeTickResult) { .length = dm_tick_length(module), .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void digmug_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const DigmugNuade* state = (const DigmugNuade*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, dm_format_name(state->module));
    if (dm_title(state->module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, dm_title(state->module));
    }
}

#define DIGMUG_TRACK_ROWS 64
// Note 23 is the period 853 (C-1 in ProTracker terms); note n shows as semitone n + 1 with the octave one lower.
#define DIGMUG_OCTAVE_SHIFT (-1)

static const DmModule* digmug_module(const void* user_data) {
    return ((const DigmugNuade*)user_data)->module;
}

static NuadePositionResult digmug_position(void* user_data) {
    const DmPosition p = dm_position(digmug_module(user_data));
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t digmug_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DmModule* module = digmug_module(user_data);
    const DmPosition p = dm_position(module);
    const uint32_t count = (uint32_t)dm_channel_count(module);
    const uint32_t n = cap < count ? cap : count;
    for (uint32_t i = 0; i < n; i++) {
        const DmVoiceState v = dm_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note >= 0 ? v.note + 1 : -1, // a transpose below the table plays nothing
                                    .instrument = v.instrument,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult digmug_tracker_info(void* user_data, const NuadePosition* at) {
    const DmModule* module = digmug_module(user_data);
    if ((int)at->order >= dm_position_count(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = (uint32_t)dm_channel_count(module),
                                                .rows = at->rows != 0 ? at->rows : DIGMUG_TRACK_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, DIGMUG_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell digmug_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const DmPatternCell c = dm_pattern_cell((const DmModule*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        cell.value[0] = c.note + 1;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t digmug_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                     int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const DmModule* module = digmug_module(user_data);
    if ((int)at->order >= dm_position_count(module)) {
        return 0;
    }
    return nuade_tracker_fill(at, (uint32_t)dm_channel_count(module), at->rows != 0 ? at->rows : DIGMUG_TRACK_ROWS,
                              channel, row_lo, row_hi, out, cap, digmug_cell, module);
}

const NuadePlayer nuade_player_digmug = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "digmug",
    .name = "Digital Mugician",
    .version = "1",
    .extensions = "dmu,dmu2,mug,mug2",
    .probe = digmug_probe,
    .open = digmug_open,
    .close = digmug_close,
    .start = digmug_start,
    .tick = digmug_tick,
    .metadata = digmug_metadata,
    .position = digmug_position,
    .voice_info = digmug_voice_info,
    .tracker_info = digmug_tracker_info,
    .tracker_cells = digmug_tracker_cells,
};

DmModule* dm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_digmug) {
        return nullptr;
    }
    return ((DigmugNuade*)nuade_player_data(song))->module;
}
