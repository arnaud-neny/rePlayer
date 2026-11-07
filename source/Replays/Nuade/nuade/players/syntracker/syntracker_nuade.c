// SPDX-License-Identifier: MIT
// SynTracker as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/syntracker.h>
#include <stdio.h>

// A note is a period table index; index 36 (period 854) is C-1, so the column starts three octaves below it at index 0.
#define STK_OCTAVE_SHIFT (-2)

static NuadeProbe syntracker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return stk_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult syntracker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    StkPlayer* player = stk_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid SynTracker module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = STK_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = STK_TIMEBASE_HZ
    };
    return result;
}

static void syntracker_close(void* user_data) {
    stk_destroy((StkPlayer*)user_data);
}

static bool syntracker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    (void)voices;
    if (subsong != 0) {
        return false;
    }
    stk_start((StkPlayer*)user_data);
    return true;
}

static NuadeTickResult syntracker_tick(void* user_data, NuadeVoices* voices) {
    const StkTickResult tick = stk_tick((StkPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = STK_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void syntracker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const StkPlayer* player = (const StkPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "SynTracker");
    for (int i = 0; i < STK_INSTRUMENTS; i++) {
        const char* name = stk_instrument_name(player, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static NuadePositionResult syntracker_position(void* user_data) {
    const StkPosition p = stk_position((const StkPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = STK_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t syntracker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const StkPlayer* player = (const StkPlayer*)user_data;
    const StkPosition p = stk_position(player);
    const uint32_t n = cap < STK_VOICES ? cap : STK_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const StkVoiceState v = stk_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note,
                                    .instrument = v.instrument > 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult syntracker_tracker_info(void* user_data, const NuadePosition* at) {
    const StkPlayer* player = (const StkPlayer*)user_data;
    if ((int)at->order >= stk_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = STK_VOICES,
                                                .rows = STK_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, STK_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "fx", NuadeColumn_Hex, 2, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell syntracker_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const StkCell c = stk_pattern_cell((const StkPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        cell.value[0] = c.note - 1;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.command != 0) {
        cell.value[2] = c.command;
    }
    if (c.param != 0) {
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t syntracker_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                         int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const StkPlayer* player = (const StkPlayer*)user_data;
    if ((int)at->order >= stk_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, STK_VOICES, STK_ROWS, channel, row_lo, row_hi, out, cap, syntracker_cell, user_data);
}

const NuadePlayer nuade_player_syntracker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "syntracker",
    .name = "SynTracker",
    .version = "1",
    .extensions = "synmod,st",
    .probe = syntracker_probe,
    .open = syntracker_open,
    .close = syntracker_close,
    .start = syntracker_start,
    .tick = syntracker_tick,
    .metadata = syntracker_metadata,
    .position = syntracker_position,
    .voice_info = syntracker_voice_info,
    .tracker_info = syntracker_tracker_info,
    .tracker_cells = syntracker_tracker_cells,
};

StkPlayer* stk_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_syntracker) {
        return nullptr;
    }
    return (StkPlayer*)nuade_player_data(song);
}
