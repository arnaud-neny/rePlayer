// SPDX-License-Identifier: MIT
// Fashion Tracker as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/fashiontracker.h>
#include <stdio.h>

static NuadeProbe fashiontracker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ft_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult fashiontracker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    FtPlayer* player = ft_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Fashion Tracker module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = FT_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = FT_TIMEBASE_HZ
    };
    return result;
}

static void fashiontracker_close(void* user_data) {
    ft_destroy((FtPlayer*)user_data);
}

static bool fashiontracker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ft_start((FtPlayer*)user_data, voices);
}

static NuadeTickResult fashiontracker_tick(void* user_data, NuadeVoices* voices) {
    const FtTickResult tick = ft_tick((FtPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = FT_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void fashiontracker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Fashion Tracker");
}

static NuadePositionResult fashiontracker_position(void* user_data) {
    const FtPosition p = ft_position((const FtPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = FT_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t fashiontracker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const FtPlayer* player = (const FtPlayer*)user_data;
    const FtPosition p = ft_position(player);
    const uint32_t n = cap < FT_VOICES ? cap : FT_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const FtVoiceState v = ft_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)p.order, .row = (uint32_t)p.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult fashiontracker_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= ft_order_count((const FtPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = FT_VOICES,
                                                .rows = FT_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "inst", NuadeColumn_Hex, 1, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell fashiontracker_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const FtCell c = ft_pattern_cell((const FtPlayer*)ctx, (int)order, (int)channel, (int)row);
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

static uint32_t fashiontracker_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                             int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const FtPlayer* player = (const FtPlayer*)user_data;
    if ((int)at->order >= ft_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, FT_VOICES, FT_ROWS, channel, row_lo, row_hi, out, cap, fashiontracker_cell,
                              user_data);
}

const NuadePlayer nuade_player_fashiontracker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "fashiontracker",
    .name = "Fashion Tracker",
    .version = "1",
    .extensions = "ex",
    .probe = fashiontracker_probe,
    .open = fashiontracker_open,
    .close = fashiontracker_close,
    .start = fashiontracker_start,
    .tick = fashiontracker_tick,
    .metadata = fashiontracker_metadata,
    .position = fashiontracker_position,
    .voice_info = fashiontracker_voice_info,
    .tracker_info = fashiontracker_tracker_info,
    .tracker_cells = fashiontracker_tracker_cells,
};

FtPlayer* ft_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_fashiontracker) {
        return nullptr;
    }
    return (FtPlayer*)nuade_player_data(song);
}
