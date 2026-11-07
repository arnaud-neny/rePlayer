// SPDX-License-Identifier: MIT
// MMDC player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/mmdc.h>
#include <stdio.h>

// Note 1 is the period table entry 856, C-1 = 12.
#define NOTE_BASE 11

static NuadeProbe mmdc_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return mmdc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult mmdc_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    MmdcPlayer* player = mmdc_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid MMDC module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = MMDC_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = MMDC_TIMEBASE_HZ
    };
    return result;
}

static void mmdc_nuade_close(void* user_data) {
    mmdc_destroy((MmdcPlayer*)user_data);
}

static bool mmdc_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && mmdc_start((MmdcPlayer*)user_data, voices);
}

static NuadeTickResult mmdc_nuade_tick(void* user_data, NuadeVoices* voices) {
    const MmdcTickResult tick = mmdc_tick((MmdcPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void mmdc_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "OctaMED MMDC");
}

static uint32_t mmdc_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const MmdcPlayer* player = (const MmdcPlayer*)user_data;
    const MmdcPosition position = mmdc_position(player);
    const uint32_t n = cap < MMDC_VOICES ? cap : MMDC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const MmdcVoiceState v = mmdc_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = position.order, .row = position.line
        };
    }
    return n;
}

static NuadePositionResult mmdc_nuade_position(void* user_data) {
    const MmdcPosition p = mmdc_position((const MmdcPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = p.order,
                                                 .pattern = p.block,
                                                 .row = p.line,
                                                 .rows = p.lines,
                                                 .tick = p.tick,
                                                 .speed = MMDC_TICKS_PER_LINE } };
}

static NuadeTrackerInfoResult mmdc_nuade_tracker_info(void* user_data, const NuadePosition* at) {
    MmdcBlockShape shape;
    if (!mmdc_block_shape((const MmdcPlayer*)user_data, at->order, &shape)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = MMDC_VOICES,
                                                .rows = shape.lines,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell mmdc_nuade_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const MmdcCell c = mmdc_pattern_cell((const MmdcPlayer*)ctx, order, channel, row);
    if (c.note != 0) {
        cell.value[0] = c.note + NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.param != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t mmdc_nuade_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                         int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const MmdcPlayer* player = (const MmdcPlayer*)user_data;
    MmdcBlockShape shape;
    if (!mmdc_block_shape(player, at->order, &shape)) {
        return 0;
    }
    return nuade_tracker_fill(at, MMDC_VOICES, shape.lines, channel, row_lo, row_hi, out, cap, mmdc_nuade_cell, player);
}

const NuadePlayer nuade_player_mmdc = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "mmdc",
    .name = "OctaMED MMDC",
    .version = "1",
    .extensions = "mmdc",
    .probe = mmdc_nuade_probe,
    .open = mmdc_nuade_open,
    .close = mmdc_nuade_close,
    .start = mmdc_nuade_start,
    .tick = mmdc_nuade_tick,
    .metadata = mmdc_nuade_metadata,
    .voice_info = mmdc_nuade_voice_info,
    .position = mmdc_nuade_position,
    .tracker_info = mmdc_nuade_tracker_info,
    .tracker_cells = mmdc_nuade_tracker_cells,
};

MmdcPlayer* mmdc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_mmdc) {
        return nullptr;
    }
    return (MmdcPlayer*)nuade_player_data(song);
}
