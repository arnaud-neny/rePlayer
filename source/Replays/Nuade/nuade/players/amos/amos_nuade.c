// SPDX-License-Identifier: MIT
// AMOS Music AmosBank as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/amos.h>
#include <stdio.h>

// The note index 0 of the period table is C-1.
#define AMOS_NOTE_BASE 12

static NuadeProbe amos_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return amos_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult amos_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    AmosPlayer* player = amos_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid AMOS music bank");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = AMOS_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = AMOS_TIMEBASE_HZ
    };
    return result;
}

static void amos_nuade_close(void* user_data) {
    amos_destroy((AmosPlayer*)user_data);
}

static bool amos_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && amos_start((AmosPlayer*)user_data, voices);
}

static NuadeTickResult amos_nuade_tick(void* user_data, NuadeVoices* voices) {
    const AmosTickResult tick = amos_tick((AmosPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void amos_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const AmosPlayer* player = (const AmosPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "AMOS Music AmosBank");
    if (amos_title(player)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, amos_title(player));
    }
    for (int i = 0; i < AMOS_SAMPLES; i++) {
        const char* name = amos_sample_name(player, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static NuadePositionResult amos_nuade_position(void* user_data) {
    const AmosPosition p = amos_position((const AmosPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = AMOS_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t amos_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const AmosPlayer* player = (const AmosPlayer*)user_data;
    const AmosPosition p = amos_position(player);
    const uint32_t n = cap < AMOS_VOICES ? cap : AMOS_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const AmosVoiceState v = amos_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note >= 0 ? v.note + AMOS_NOTE_BASE : -1,
                                    .instrument = v.sample > 0 ? v.sample - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult amos_nuade_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= amos_order_count((const AmosPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = AMOS_VOICES,
                                                .rows = AMOS_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "smp", NuadeColumn_Hex, 2, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell amos_nuade_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const AmosCell c = amos_pattern_cell((const AmosPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.period != 0) {
        cell.value[0] = nuade_period_to_note(c.period);
    }
    if (c.sample != 0) {
        cell.value[1] = c.sample;
    }
    if (c.effect != 0 || c.param != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t amos_nuade_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                         int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if ((int)at->order >= amos_order_count((const AmosPlayer*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, AMOS_VOICES, AMOS_ROWS, channel, row_lo, row_hi, out, cap, amos_nuade_cell,
                              user_data);
}

const NuadePlayer nuade_player_amos = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "amos",
    .name = "AMOS Music AmosBank",
    .version = "1",
    .extensions = "abk",
    .probe = amos_nuade_probe,
    .open = amos_nuade_open,
    .close = amos_nuade_close,
    .start = amos_nuade_start,
    .tick = amos_nuade_tick,
    .metadata = amos_nuade_metadata,
    .position = amos_nuade_position,
    .voice_info = amos_nuade_voice_info,
    .tracker_info = amos_nuade_tracker_info,
    .tracker_cells = amos_nuade_tracker_cells,
};

AmosPlayer* amos_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_amos) {
        return nullptr;
    }
    return (AmosPlayer*)nuade_player_data(song);
}
