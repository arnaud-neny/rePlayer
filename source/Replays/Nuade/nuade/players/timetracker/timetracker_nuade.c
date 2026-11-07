// SPDX-License-Identifier: MIT
// TimeTracker as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/timetracker.h>
#include <stdio.h>

// Note n is period table entry n; entry 1 (856) is C-1.
#define TT_NOTE_BASE 11

static NuadeProbe timetracker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return tt_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult timetracker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const TtCreateResult created = tt_create(source->data, source->size, source->path, source->io);
    switch (created.status) {
        case TtStatus_Ok:
            result.user_data = created.player;
            result.info = (NuadeSongInfo) { .voice_count = TT_VOICES,
                                            .subsong_count = (uint32_t)tt_subsong_count(created.player),
                                            .default_subsong = 0,
                                            .timebase_hz = TT_TIMEBASE_HZ };
            break;
        case TtStatus_MissingSamples:
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "sample file \"%.90s\" not found",
                     created.missing_file);
            break;
        case TtStatus_OutOfMemory:
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
            break;
        case TtStatus_InvalidData:
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message), "not a valid TimeTracker module");
            break;
    }
    return result;
}

static void timetracker_close(void* user_data) {
    tt_destroy((TtPlayer*)user_data);
}

static bool timetracker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return tt_start((TtPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult timetracker_tick(void* user_data, NuadeVoices* voices) {
    const TtTickResult tick = tt_tick((TtPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = TT_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void timetracker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "TimeTracker");
}

static NuadePositionResult timetracker_position(void* user_data) {
    const TtPosition p = tt_position((const TtPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = TT_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t timetracker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const TtPlayer* player = (const TtPlayer*)user_data;
    const TtPosition p = tt_position(player);
    const uint32_t n = cap < TT_VOICES ? cap : TT_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const TtVoiceState v = tt_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note + TT_NOTE_BASE : -1,
                                    .instrument = v.sample > 0 ? v.sample - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult timetracker_tracker_info(void* user_data, const NuadePosition* at) {
    const TtPlayer* player = (const TtPlayer*)user_data;
    if ((int)at->order >= tt_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = TT_VOICES,
                                                .rows = TT_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "smp", NuadeColumn_Hex, 2, 0 },
                                                             { "cmd", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell timetracker_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const TtCell c = tt_pattern_cell((const TtPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        cell.value[0] = c.note + TT_NOTE_BASE;
    }
    if (c.sample != 0) {
        cell.value[1] = c.sample;
    }
    if (c.cmd != 0 || c.param != 0) {
        cell.value[2] = c.cmd;
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t timetracker_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const TtPlayer* player = (const TtPlayer*)user_data;
    if ((int)at->order >= tt_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, TT_VOICES, TT_ROWS, channel, row_lo, row_hi, out, cap, timetracker_cell, user_data);
}

const NuadePlayer nuade_player_timetracker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "timetracker",
    .name = "TimeTracker",
    .version = "1",
    .extensions = "tmk",
    .probe = timetracker_probe,
    .open = timetracker_open,
    .close = timetracker_close,
    .start = timetracker_start,
    .tick = timetracker_tick,
    .metadata = timetracker_metadata,
    .position = timetracker_position,
    .voice_info = timetracker_voice_info,
    .tracker_info = timetracker_tracker_info,
    .tracker_cells = timetracker_tracker_cells,
};

TtPlayer* tt_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_timetracker) {
        return nullptr;
    }
    return (TtPlayer*)nuade_player_data(song);
}
