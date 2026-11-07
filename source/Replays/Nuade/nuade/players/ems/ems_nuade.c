// SPDX-License-Identifier: MIT
// EMS (Editeur Musical Sequentiel) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/ems.h>
#include <stdio.h>

static NuadeProbe ems_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ems_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult ems_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    EmsPlayer* player = ems_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid EMS module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = EMS_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = EMS_TIMEBASE_HZ
    };
    return result;
}

static void ems_nuade_close(void* user_data) {
    ems_destroy((EmsPlayer*)user_data);
}

static bool ems_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ems_start((EmsPlayer*)user_data, voices);
}

static NuadeTickResult ems_nuade_tick(void* user_data, NuadeVoices* voices) {
    const EmsTickResult tick = ems_tick((EmsPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = EMS_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void ems_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "EMS (Editeur Musical Sequentiel)");
}

static NuadePositionResult ems_nuade_position(void* user_data) {
    const EmsPosition p = ems_position((const EmsPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = EMS_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t ems_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const EmsPlayer* player = (const EmsPlayer*)user_data;
    const EmsPosition p = ems_position(player);
    const uint32_t n = cap < EMS_VOICES ? cap : EMS_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const EmsVoiceState v = ems_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period),
                                    .instrument = v.instrument > 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult ems_nuade_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= ems_order_count((const EmsPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = EMS_VOICES,
                                                .rows = EMS_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "inst", NuadeColumn_Hex, 1, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell ems_nuade_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const EmsCell c = ems_pattern_cell((const EmsPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.period != 0) {
        cell.value[0] = nuade_period_to_note(c.period);
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0) {
        cell.value[2] = c.effect;
    }
    if (c.param != 0) {
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t ems_nuade_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if ((int)at->order >= ems_order_count((const EmsPlayer*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, EMS_VOICES, EMS_ROWS, channel, row_lo, row_hi, out, cap, ems_nuade_cell, user_data);
}

const NuadePlayer nuade_player_ems = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "ems",
    .name = "EMS (Editeur Musical Sequentiel)",
    .version = "1",
    .extensions = "ems",
    .probe = ems_nuade_probe,
    .open = ems_nuade_open,
    .close = ems_nuade_close,
    .start = ems_nuade_start,
    .tick = ems_nuade_tick,
    .metadata = ems_nuade_metadata,
    .position = ems_nuade_position,
    .voice_info = ems_nuade_voice_info,
    .tracker_info = ems_nuade_tracker_info,
    .tracker_cells = ems_nuade_tracker_cells,
};

EmsPlayer* ems_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_ems) {
        return nullptr;
    }
    return (EmsPlayer*)nuade_player_data(song);
}
