// SPDX-License-Identifier: MIT
// Tronic as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/tronic.h>
#include <stdio.h>

static NuadeProbe tronic_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return tronic_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult tronic_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    TronicPlayer* player = tronic_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Tronic module this player models");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = TRONIC_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = TRONIC_TIMEBASE_HZ
    };
    return result;
}

static void tronic_nuade_close(void* user_data) {
    tronic_destroy((TronicPlayer*)user_data);
}

static bool tronic_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return tronic_start((TronicPlayer*)user_data, voices);
}

static NuadeTickResult tronic_nuade_tick(void* user_data, NuadeVoices* voices) {
    const TronicTickResult tick = tronic_tick((TronicPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = TRONIC_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void tronic_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Tronic");
}

static NuadePositionResult tronic_nuade_position(void* user_data) {
    const TronicPosition p = tronic_position((const TronicPlayer*)user_data);
    return (NuadePositionResult) {
        .valid = true,
        .position
        = { .mode = NuadeScroll_Rows, .order = (uint32_t)p.order, .row = (uint32_t)p.row, .speed = (uint32_t)p.speed }
    };
}

static uint32_t tronic_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const TronicPlayer* player = (const TronicPlayer*)user_data;
    const TronicPosition p = tronic_position(player);
    const uint32_t n = cap < TRONIC_VOICES ? cap : TRONIC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const TronicVoiceState v = tronic_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.period < 0 ? -1 : nuade_period_to_note((uint32_t)v.period),
                                    .instrument = v.instrument,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult tronic_nuade_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= tronic_order_count((const TronicPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = TRONIC_VOICES,
                                                .rows = TRONIC_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, -2 },
                                                             { "inst", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell tronic_nuade_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const TronicCell c = tronic_pattern_cell((const TronicPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        // Period-table index 1 is the lowest note, two octaves below C-0 (the column's octave shift).
        cell.value[0] = c.note - 1;
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t tronic_nuade_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                           int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if ((int)at->order >= tronic_order_count((const TronicPlayer*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, TRONIC_VOICES, TRONIC_ROWS, channel, row_lo, row_hi, out, cap, tronic_nuade_cell,
                              user_data);
}

const NuadePlayer nuade_player_tronic = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "tronic",
    .name = "Tronic",
    .version = "1",
    .extensions = "dp,trc,tro,tronic,tron",
    .probe = tronic_nuade_probe,
    .open = tronic_nuade_open,
    .close = tronic_nuade_close,
    .start = tronic_nuade_start,
    .tick = tronic_nuade_tick,
    .metadata = tronic_nuade_metadata,
    .position = tronic_nuade_position,
    .voice_info = tronic_nuade_voice_info,
    .tracker_cells = tronic_nuade_tracker_cells,
    .tracker_info = tronic_nuade_tracker_info,
};

TronicPlayer* tronic_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_tronic) {
        return nullptr;
    }
    return (TronicPlayer*)nuade_player_data(song);
}
