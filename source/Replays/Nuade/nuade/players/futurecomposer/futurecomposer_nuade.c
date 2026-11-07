// SPDX-License-Identifier: MIT
// Future Composer 1.0-1.4 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/futurecomposer.h>
#include <stdio.h>

// The "FC14" and "SMOD" marks are specific enough to be sure; SMOD must also convert.
static NuadeProbe futurecomposer_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return fc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult futurecomposer_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    FcModule* module = fc_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Future Composer module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = FC_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = FC_TICK_HZ
    };
    return result;
}

static void futurecomposer_close(void* user_data) {
    fc_destroy((FcModule*)user_data);
}

static bool futurecomposer_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return fc_start((FcModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult futurecomposer_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = fc_tick((FcModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void futurecomposer_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, fc_format_name((const FcModule*)user_data));
}

// Voice 0 stands for the song; the other voices differ in NuadeVoiceInfo.
static NuadePositionResult futurecomposer_position(void* user_data) {
    const FcVoiceState v = fc_voice_state((const FcModule*)user_data, 0);
    return (NuadePositionResult) {
        .valid = true,
        .position
        = { .mode = NuadeScroll_Rows, .order = (uint32_t)v.order, .row = (uint32_t)v.row, .speed = (uint32_t)v.speed }
    };
}

static uint32_t futurecomposer_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const uint32_t n = cap < FC_VOICES ? cap : FC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const FcVoiceState v = fc_voice_state((const FcModule*)user_data, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)v.order, .row = (uint32_t)v.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult futurecomposer_tracker_info(void* user_data, const NuadePosition* at) {
    const int rows = fc_pattern_rows((const FcModule*)user_data, (int)at->order);
    if (rows == 0) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = FC_VOICES,
                                                .rows = (uint32_t)rows,
                                                .column_count = 3,
                                                .columns = { { "note", NuadeColumn_Note, 3, FC_NOTE_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Dec, 2 },
                                                             { "port", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell futurecomposer_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const FcCell c = fc_cell((const FcModule*)ctx, (int)order, (int)channel, (int)row);
    NuadePatternCell cell = nuade_cell_empty();
    cell.value[0] = c.note;
    cell.value[1] = c.instrument;
    if (c.flags != 0) {
        cell.value[2] = c.flags;
    }
    return cell;
}

static uint32_t futurecomposer_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                             int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const uint32_t rows = (uint32_t)fc_pattern_rows((const FcModule*)user_data, (int)at->order);
    return nuade_tracker_fill(at, FC_VOICES, rows, channel, row_lo, row_hi, out, cap, futurecomposer_cell, user_data);
}

const NuadePlayer nuade_player_futurecomposer = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "futurecomposer",
    .name = "Future Composer",
    .version = "1",
    .extensions = "fc,fc14,fc13,fc3,fc4,smod",
    .probe = futurecomposer_probe,
    .open = futurecomposer_open,
    .close = futurecomposer_close,
    .start = futurecomposer_start,
    .tick = futurecomposer_tick,
    .metadata = futurecomposer_metadata,
    .position = futurecomposer_position,
    .voice_info = futurecomposer_voice_info,
    .tracker_info = futurecomposer_tracker_info,
    .tracker_cells = futurecomposer_tracker_cells,
};

FcModule* fc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_futurecomposer) {
        return nullptr;
    }
    return (FcModule*)nuade_player_data(song);
}
