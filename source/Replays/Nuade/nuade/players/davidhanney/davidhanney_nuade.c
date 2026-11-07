// SPDX-License-Identifier: MIT
// David Hanney as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/davidhanney.h>
#include <stdio.h>

static NuadeProbe davidhanney_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dh_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult davidhanney_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DhPlayer* player = dh_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid David Hanney module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = DH_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = DH_TIMEBASE_HZ
    };
    return result;
}

static void davidhanney_close(void* user_data) {
    dh_destroy((DhPlayer*)user_data);
}

static bool davidhanney_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && dh_start((DhPlayer*)user_data, voices);
}

static NuadeTickResult davidhanney_tick(void* user_data, NuadeVoices* voices) {
    const DhTickResult tick = dh_tick((DhPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = DH_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void davidhanney_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "David Hanney");
}

static NuadePositionResult davidhanney_position(void* user_data) {
    const DhPosition p = dh_position((const DhPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t davidhanney_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DhPlayer* player = (const DhPlayer*)user_data;
    const DhPosition p = dh_position(player);
    const uint32_t n = cap < DH_VOICES ? cap : DH_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const DhVoiceState v = dh_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)p.order, .row = (uint32_t)p.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult davidhanney_tracker_info(void* user_data, const NuadePosition* at) {
    const DhPlayer* player = (const DhPlayer*)user_data;
    if ((int)at->order >= dh_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = DH_VOICES,
                                                .rows = (uint32_t)dh_pattern_rows(player, (int)at->order),
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, DH_NOTE_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 1, 0 } } } };
}

static int32_t davidhanney_note(uint8_t note) {
    switch (note) {
        case 0:
            return NUADE_CELL_EMPTY;
        case DH_NOTE_STOP:
            return NUADE_NOTE_CUT;
        case DH_NOTE_RELEASE:
            return NUADE_NOTE_OFF;
        default:
            return dh_note_value(note);
    }
}

static NuadePatternCell davidhanney_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const DhCell c = dh_pattern_cell((const DhPlayer*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = davidhanney_note(c.note);
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.param != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t davidhanney_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const DhPlayer* player = (const DhPlayer*)user_data;
    if ((int)at->order >= dh_order_count(player)) {
        return 0;
    }
    const uint32_t rows = (uint32_t)dh_pattern_rows(player, (int)at->order);
    return nuade_tracker_fill(at, DH_VOICES, rows, channel, row_lo, row_hi, out, cap, davidhanney_cell, user_data);
}

const NuadePlayer nuade_player_davidhanney = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "davidhanney",
    .name = "David Hanney",
    .version = "1",
    .extensions = "dh",
    .probe = davidhanney_probe,
    .open = davidhanney_open,
    .close = davidhanney_close,
    .start = davidhanney_start,
    .tick = davidhanney_tick,
    .metadata = davidhanney_metadata,
    .position = davidhanney_position,
    .voice_info = davidhanney_voice_info,
    .tracker_info = davidhanney_tracker_info,
    .tracker_cells = davidhanney_tracker_cells,
};

DhPlayer* dh_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_davidhanney) {
        return nullptr;
    }
    return (DhPlayer*)nuade_player_data(song);
}
