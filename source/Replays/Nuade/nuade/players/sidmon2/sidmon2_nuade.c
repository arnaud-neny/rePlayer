// SPDX-License-Identifier: MIT
// SidMon II as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/sidmon2.h>
#include <stdio.h>

// The module carries a text mark at a fixed offset, specific enough to be sure.
static NuadeProbe sidmon2_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sd2_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult sidmon2_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Sd2Module* module = sd2_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid SidMon II module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = SD2_VOICES,
                                    .subsong_count = (uint32_t)sd2_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = SD2_TICK_HZ };
    return result;
}

static void sidmon2_close(void* user_data) {
    sd2_destroy((Sd2Module*)user_data);
}

static bool sidmon2_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sd2_start((Sd2Module*)user_data, (int)subsong, voices);
}

static NuadeTickResult sidmon2_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = sd2_tick((Sd2Module*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void sidmon2_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "SidMon II");
}

static NuadePositionResult sidmon2_position(void* user_data) {
    const Sd2VoiceState v = sd2_voice_state((const Sd2Module*)user_data, 0);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)v.order,
                                                 .row = (uint32_t)v.row,
                                                 .rows = (uint32_t)sd2_pattern_rows((const Sd2Module*)user_data),
                                                 .speed = (uint32_t)v.speed } };
}

static uint32_t sidmon2_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const uint32_t n = cap < SD2_VOICES ? cap : SD2_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Sd2VoiceState v = sd2_voice_state((const Sd2Module*)user_data, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)v.order, .row = (uint32_t)v.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult sidmon2_tracker_info(void* user_data, const NuadePosition* at) {
    const Sd2Module* module = (const Sd2Module*)user_data;
    if ((int)at->order >= sd2_order_count(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = SD2_VOICES,
                                                .rows = (uint32_t)sd2_pattern_rows(module),
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, SD2_NOTE_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Dec, 2 },
                                                             { "fx", NuadeColumn_Hex, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell sidmon2_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const Sd2Cell c = sd2_pattern_cell((const Sd2Module*)ctx, (int)order, (int)channel, (int)row);
    NuadePatternCell cell = nuade_cell_empty();
    cell.value[0] = c.note;
    cell.value[1] = c.instrument;
    cell.value[2] = c.effect;
    cell.value[3] = c.effect >= 0 ? c.param : -1;
    return cell;
}

static uint32_t sidmon2_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                      int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const uint32_t rows = (uint32_t)sd2_pattern_rows((const Sd2Module*)user_data);
    return nuade_tracker_fill(at, SD2_VOICES, rows, channel, row_lo, row_hi, out, cap, sidmon2_cell, user_data);
}

const NuadePlayer nuade_player_sidmon2 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "sidmon2",
    .name = "SidMon II",
    .version = "1",
    .extensions = "sid2,sd2", // rePlayer
    .probe = sidmon2_probe,
    .open = sidmon2_open,
    .close = sidmon2_close,
    .start = sidmon2_start,
    .tick = sidmon2_tick,
    .metadata = sidmon2_metadata,
    .position = sidmon2_position,
    .voice_info = sidmon2_voice_info,
    .tracker_info = sidmon2_tracker_info,
    .tracker_cells = sidmon2_tracker_cells,
};

Sd2Module* sd2_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_sidmon2) {
        return nullptr;
    }
    return (Sd2Module*)nuade_player_data(song);
}
