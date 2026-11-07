// SPDX-License-Identifier: MIT
// Digital Sound Studio (MMU2) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/dss.h>
#include <stdio.h>

#define DSS_NUADE_VOICES 4
#define DSS_NUADE_INSTRUMENTS 31
#define DSS_NUADE_ROWS 64

// The module mark is specific enough to be sure.
static NuadeProbe dss_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dss_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult dss_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DssModule* module = dss_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Digital Sound Studio module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = DSS_NUADE_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = DSS_TIMEBASE_HZ
    };
    return result;
}

static void dss_nuade_close(void* user_data) {
    dss_destroy((DssModule*)user_data);
}

static bool dss_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dss_start((DssModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult dss_nuade_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    uint32_t length = 0;
    const bool looped = dss_tick((DssModule*)user_data, &length);
    return (NuadeTickResult) { .length = length, .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void dss_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const DssModule* module = (const DssModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, dss_format_name(module));
    for (uint32_t i = 0; i < DSS_NUADE_INSTRUMENTS; i++) {
        const char* name = dss_get_instrument_name(module, (int)i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, i, name);
        }
    }
}

static NuadePositionResult dss_nuade_position(void* user_data) {
    const DssPosition p = dss_position((const DssModule*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .speed = (uint32_t)p.speed,
                                                 .tempo = (uint32_t)p.tempo } };
}

static uint32_t dss_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DssModule* module = (const DssModule*)user_data;
    const DssPosition p = dss_position(module);
    const uint32_t n = cap < DSS_NUADE_VOICES ? cap : DSS_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const DssVoiceState v = dss_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period),
                                    .instrument = v.sample != 0 ? v.sample - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult dss_nuade_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= dss_get_num_positions((const DssModule*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = DSS_NUADE_VOICES,
                                                .rows = DSS_NUADE_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "inst", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell dss_nuade_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const DssCell c = dss_pattern_cell((const DssModule*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = c.period == DSS_PERIOD_CUT ? NUADE_NOTE_CUT : nuade_period_to_note(c.period);
    if (c.sample != 0) {
        cell.value[1] = c.sample;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t dss_nuade_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if ((int)at->order >= dss_get_num_positions((const DssModule*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, DSS_NUADE_VOICES, DSS_NUADE_ROWS, channel, row_lo, row_hi, out, cap, dss_nuade_cell,
                              user_data);
}

const NuadePlayer nuade_player_dss = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "dss",
    .name = "Digital Sound Studio",
    .version = "1",
    .extensions = "dss",
    .probe = dss_nuade_probe,
    .open = dss_nuade_open,
    .close = dss_nuade_close,
    .start = dss_nuade_start,
    .tick = dss_nuade_tick,
    .metadata = dss_nuade_metadata,
    .position = dss_nuade_position,
    .voice_info = dss_nuade_voice_info,
    .tracker_info = dss_nuade_tracker_info,
    .tracker_cells = dss_nuade_tracker_cells,
};

DssModule* dss_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_dss) {
        return nullptr;
    }
    return (DssModule*)nuade_player_data(song);
}
