// SPDX-License-Identifier: MIT
// Tomy Tracker as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/tomytracker.h>
#include <stdio.h>

static NuadeProbe tomytracker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return tmy_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult tomytracker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    TmyPlayer* player = tmy_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Tomy Tracker module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = TMY_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = TMY_TIMEBASE_HZ
    };
    return result;
}

static void tomytracker_close(void* user_data) {
    tmy_destroy((TmyPlayer*)user_data);
}

static bool tomytracker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && tmy_start((TmyPlayer*)user_data, voices);
}

static NuadeTickResult tomytracker_tick(void* user_data, NuadeVoices* voices) {
    const TmyTickResult tick = tmy_tick((TmyPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = TMY_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void tomytracker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Tomy Tracker");
}

static NuadePositionResult tomytracker_position(void* user_data) {
    const TmyPosition p = tmy_position((const TmyPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = TMY_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t tomytracker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const TmyPlayer* player = (const TmyPlayer*)user_data;
    const TmyPosition p = tmy_position(player);
    const uint32_t n = cap < TMY_VOICES ? cap : TMY_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const TmyVoiceState v = tmy_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)p.order, .row = (uint32_t)p.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult tomytracker_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= tmy_order_count((const TmyPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = TMY_VOICES,
                                                .rows = TMY_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell tomytracker_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const TmyCell c = tmy_pattern_cell((const TmyPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.note >= 0) {
        cell.value[0] = c.note;
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

static uint32_t tomytracker_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const TmyPlayer* player = (const TmyPlayer*)user_data;
    if ((int)at->order >= tmy_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, TMY_VOICES, TMY_ROWS, channel, row_lo, row_hi, out, cap, tomytracker_cell, user_data);
}

const NuadePlayer nuade_player_tomytracker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "tomytracker",
    .name = "Tomy Tracker",
    .version = "1",
    .extensions = "sg",
    .probe = tomytracker_probe,
    .open = tomytracker_open,
    .close = tomytracker_close,
    .start = tomytracker_start,
    .tick = tomytracker_tick,
    .metadata = tomytracker_metadata,
    .position = tomytracker_position,
    .voice_info = tomytracker_voice_info,
    .tracker_info = tomytracker_tracker_info,
    .tracker_cells = tomytracker_tracker_cells,
};

TmyPlayer* tmy_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_tomytracker) {
        return nullptr;
    }
    return (TmyPlayer*)nuade_player_data(song);
}
